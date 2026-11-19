# Copyright Martin Raiber. All Rights Reserved.
# SPDX-License-Identifier: LGPL-3.0-or-later
"""
Docker registry (CNCF Distribution) with S3 storage benchmark.

A registry container is started with its S3 storage driver pointing at the S3
server under test. Images with random layers on top of a shared base image are
loaded into the local Docker daemon, then pushed to the registry with
`docker push` and pulled again with `docker pull` (after removing them
locally), concurrently. Pulls follow the registry's redirect to a presigned S3
URL.

Garage is left out, as it has issues being used as Docker registry storage.
"""
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager
from dataclasses import dataclass
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tarfile
import time
from collections.abc import Iterator
from typing import Optional
from urllib.parse import urlsplit
import uuid

import pytest
import requests
from pytest_benchmark.fixture import BenchmarkFixture

from minio_fixture import minio, MinioRunner
from hs5_fixture import hs5_perf, hs5_perf_no_wal, Hs5Runner
from rustfs_fixture import rustfs, RustfsRunner
from test_perf_litestream import S3Target, s3_target

REGISTRY_IMAGE = os.getenv("REGISTRY_IMAGE", "registry:3")

OCI_MANIFEST = "application/vnd.oci.image.manifest.v1+json"
OCI_CONFIG = "application/vnd.oci.image.config.v1+json"
OCI_LAYER = "application/vnd.oci.image.layer.v1.tar+gzip"

# Unique layers per image. The largest is above the registry S3 driver's
# default 10 MiB chunk size, so it is stored using a multipart upload
# (layers are gzipped, but random data stays about the same size).
LAYER_SIZES = [4 * 1024, 256 * 1024, 2 * 1024 * 1024, 16 * 1024 * 1024]
BASE_LAYER_SIZE = 4 * 1024 * 1024


def docker_available() -> bool:
    if shutil.which("docker") is None:
        return False
    return (
        subprocess.run(
            ["docker", "info"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        ).returncode
        == 0
    )


pytestmark = pytest.mark.skipif(
    not docker_available(), reason="Docker is not available"
)


def free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


@contextmanager
def registry(target: Optional[S3Target]) -> Iterator[str]:
    """
    Run a registry container. Stores into `target` or, if None, into the
    container's local filesystem (baseline). Host networking, so the registry
    reaches the S3 server on 127.0.0.1 and S3 redirect URLs are valid for the
    client as well.
    """
    port = free_port()
    env = {
        "REGISTRY_HTTP_ADDR": f"127.0.0.1:{port}",
        # Default config binds the debug server to :5001, which clashes with
        # parallel test runs
        "REGISTRY_HTTP_DEBUG_ADDR": f"127.0.0.1:{free_port()}",
        "REGISTRY_HTTP_SECRET": uuid.uuid4().hex,
        "REGISTRY_LOG_LEVEL": "warn",
        "REGISTRY_LOG_ACCESSLOG_DISABLED": "true",
    }
    if target is not None:
        env.update(
            {
                "REGISTRY_STORAGE": "s3",
                "REGISTRY_STORAGE_S3_REGION": target.region,
                "REGISTRY_STORAGE_S3_REGIONENDPOINT": target.endpoint,
                "REGISTRY_STORAGE_S3_BUCKET": target.bucket,
                "REGISTRY_STORAGE_S3_ACCESSKEY": target.access_key,
                "REGISTRY_STORAGE_S3_SECRETKEY": target.secret_key,
                "REGISTRY_STORAGE_S3_FORCEPATHSTYLE": "true",
                "REGISTRY_STORAGE_S3_SECURE": "false",
            }
        )

    name = f"hs5-perf-registry-{uuid.uuid4().hex[:12]}"
    args = ["docker", "run", "-d", "--name", name, "--network", "host"]
    for k, v in env.items():
        args += ["-e", f"{k}={v}"]
    args.append(REGISTRY_IMAGE)
    subprocess.run(args, check=True, stdout=subprocess.DEVNULL)

    url = f"http://127.0.0.1:{port}"
    try:
        start = time.monotonic()
        while True:
            try:
                if requests.get(f"{url}/v2/", timeout=1).status_code == 200:
                    break
            except requests.ConnectionError:
                pass
            running = subprocess.run(
                ["docker", "inspect", "-f", "{{.State.Running}}", name],
                capture_output=True,
                text=True,
            ).stdout.strip()
            assert running == "true", "registry container exited"
            if time.monotonic() - start > 60:
                raise TimeoutError("Registry did not start within 60 seconds")
            time.sleep(0.1)

        yield url
    finally:
        subprocess.run(["docker", "logs", "--tail", "50", name])
        subprocess.run(
            ["docker", "rm", "-f", name],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )


@dataclass
class Layer:
    gz: bytes
    digest: str
    diff_id: str

    @staticmethod
    def make(size: int) -> "Layer":
        """
        Gzipped layer tar with a single file of random (incompressible) data
        """
        buf = io.BytesIO()
        with tarfile.open(fileobj=buf, mode="w", format=tarfile.PAX_FORMAT) as tar:
            info = tarfile.TarInfo(f"data/{uuid.uuid4().hex}")
            info.size = size
            info.mtime = int(time.time())
            tar.addfile(info, io.BytesIO(os.urandom(size)))
        data = buf.getvalue()
        gz = gzip.compress(data, compresslevel=1, mtime=0)
        return Layer(
            gz,
            "sha256:" + hashlib.sha256(gz).hexdigest(),
            "sha256:" + hashlib.sha256(data).hexdigest(),
        )


@dataclass
class Image:
    ref: str
    layers: list[Layer]


@dataclass
class Workload:
    base: Image
    images: list[Image]

    @property
    def total_bytes(self) -> int:
        return sum(len(l.gz) for i in self.images for l in i.layers)

    @property
    def all_images(self) -> list[Image]:
        return [self.base] + self.images


def make_workload(registry_host: str, num_images: int) -> Workload:
    """
    Images with random (unique) data, all built on one shared base image
    """
    run_id = uuid.uuid4().hex[:8]
    base_layer = Layer.make(BASE_LAYER_SIZE)
    base = Image(f"{registry_host}/perf-{run_id}/base:latest", [base_layer])
    images = [
        Image(
            f"{registry_host}/perf-{run_id}/app{i}:latest",
            [base_layer] + [Layer.make(s) for s in LAYER_SIZES],
        )
        for i in range(num_images)
    ]
    return Workload(base, images)


def docker_image_store() -> str:
    """
    "containerd" if Docker uses the containerd image store, else "classic"
    """
    r = subprocess.run(
        ["docker", "info", "-f", "{{json .DriverStatus}}"],
        capture_output=True,
        text=True,
    )
    return "containerd" if "io.containerd.snapshotter" in r.stdout else "classic"


def docker_load(workload: Workload) -> None:
    """
    Load the images into the local Docker daemon by streaming an archive into
    `docker load`. Like `docker save` output it is an OCI image layout plus a
    legacy manifest.json. Layers are already gzipped: The containerd image
    store keeps them as-is and pushes them without compressing again (the
    classic store decompresses on load and compresses again on push).
    """
    proc = subprocess.Popen(
        ["docker", "load", "-q"], stdin=subprocess.PIPE, stdout=subprocess.DEVNULL
    )
    assert proc.stdin is not None

    def add(tar: tarfile.TarFile, name: str, data: bytes) -> None:
        info = tarfile.TarInfo(name)
        info.size = len(data)
        info.mtime = int(time.time())
        tar.addfile(info, io.BytesIO(data))

    written: set[str] = set()

    def add_blob(tar: tarfile.TarFile, data: bytes, digest: str = "") -> dict:
        digest = digest or "sha256:" + hashlib.sha256(data).hexdigest()
        path = "blobs/sha256/" + digest.removeprefix("sha256:")
        if path not in written:
            add(tar, path, data)
            written.add(path)
        return {"digest": digest, "size": len(data), "path": path}

    with tarfile.open(fileobj=proc.stdin, mode="w|") as tar:
        add(tar, "oci-layout", json.dumps({"imageLayoutVersion": "1.0.0"}).encode())
        index = []
        legacy_manifest = []
        for image in workload.all_images:
            layers = [add_blob(tar, l.gz, l.digest) for l in image.layers]
            config = add_blob(
                tar,
                json.dumps(
                    {
                        "architecture": "amd64",
                        "os": "linux",
                        "config": {},
                        "rootfs": {
                            "type": "layers",
                            "diff_ids": [l.diff_id for l in image.layers],
                        },
                    }
                ).encode(),
            )
            manifest = add_blob(
                tar,
                json.dumps(
                    {
                        "schemaVersion": 2,
                        "mediaType": OCI_MANIFEST,
                        "config": {
                            "mediaType": OCI_CONFIG,
                            "digest": config["digest"],
                            "size": config["size"],
                        },
                        "layers": [
                            {
                                "mediaType": OCI_LAYER,
                                "digest": l["digest"],
                                "size": l["size"],
                            }
                            for l in layers
                        ],
                    }
                ).encode(),
            )
            index.append(
                {
                    "mediaType": OCI_MANIFEST,
                    "digest": manifest["digest"],
                    "size": manifest["size"],
                    "annotations": {
                        "io.containerd.image.name": image.ref,
                        "org.opencontainers.image.ref.name": image.ref.rsplit(":", 1)[
                            1
                        ],
                    },
                }
            )
            legacy_manifest.append(
                {
                    "Config": config["path"],
                    "RepoTags": [image.ref],
                    "Layers": [l["path"] for l in layers],
                }
            )
        add(
            tar,
            "index.json",
            json.dumps(
                {
                    "schemaVersion": 2,
                    "mediaType": "application/vnd.oci.image.index.v1+json",
                    "manifests": index,
                }
            ).encode(),
        )
        add(tar, "manifest.json", json.dumps(legacy_manifest).encode())
    proc.stdin.close()
    assert proc.wait() == 0, "docker load failed"


def docker(*args: str) -> None:
    r = subprocess.run(["docker", *args], capture_output=True, text=True)
    assert r.returncode == 0, (args, r.stdout, r.stderr)


def docker_rmi(workload: Workload) -> None:
    subprocess.run(
        ["docker", "image", "rm", "-f"] + [i.ref for i in workload.all_images],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def push_pull(workload: Workload, concurrency: int) -> dict:
    def run_all(fn) -> float:
        start = time.perf_counter()
        with ThreadPoolExecutor(max_workers=concurrency) as ex:
            for f in [ex.submit(fn, img) for img in workload.images]:
                f.result()
        return time.perf_counter() - start

    try:
        # Push the shared base image first, like a CI pipeline building on top of it
        start = time.perf_counter()
        docker("push", "-q", workload.base.ref)
        push_s = time.perf_counter() - start
        push_s += run_all(lambda img: docker("push", "-q", img.ref))

        # Remove the local images, so pull has to download and extract all layers
        docker_rmi(workload)

        pull_s = run_all(lambda img: docker("pull", "-q", img.ref))
    finally:
        docker_rmi(workload)

    return {"push_s": push_s, "pull_s": pull_s}


def run_registry_bench(
    benchmark: BenchmarkFixture, server_name: str, target: Optional[S3Target]
) -> None:
    num_images = int(os.getenv("DOCKER_REGISTRY_IMAGES", "16"))
    concurrency = int(os.getenv("DOCKER_REGISTRY_CONCURRENCY", "8"))
    rounds = int(os.getenv("DOCKER_REGISTRY_ROUNDS", "1"))

    results: list[dict] = []
    total_bytes = 0
    store = docker_image_store()

    with registry(target) as url:

        def setup():
            nonlocal total_bytes
            # Fresh random data every round, so nothing is deduplicated
            workload = make_workload(urlsplit(url).netloc, num_images)
            total_bytes = workload.total_bytes
            docker_load(workload)
            return (workload, concurrency), {}

        def run(workload: Workload, concurrency: int):
            results.append(push_pull(workload, concurrency))

        benchmark.pedantic(run, setup=setup, rounds=rounds, iterations=1)

    mib = total_bytes / (1024 * 1024)
    push_s = min(r["push_s"] for r in results)
    pull_s = min(r["pull_s"] for r in results)
    stats = {
        "images": num_images,
        "concurrency": concurrency,
        "docker_image_store": store,
        "mib_per_round": mib,
        "push_s": push_s,
        "pull_s": pull_s,
        "push_mib_s": mib / push_s,
        "pull_mib_s": mib / pull_s,
        "push_images_s": num_images / push_s,
    }
    benchmark.extra_info.update(stats)
    print(
        f"\nDOCKER REGISTRY {server_name} store={store} images={num_images} size={mib:.0f}MiB "
        f"concurrency={concurrency} push={push_s:.2f}s ({stats['push_mib_s']:.1f} MiB/s) "
        f"pull={pull_s:.2f}s ({stats['pull_mib_s']:.1f} MiB/s)"
    )


def test_perf_docker_registry_filesystem(benchmark: BenchmarkFixture):
    """
    Baseline: registry storing into its local filesystem
    """
    run_registry_bench(benchmark, "filesystem", None)


def run_hs5_registry_bench(
    benchmark: BenchmarkFixture, server_name: str, hs5: Hs5Runner
) -> None:
    rss_before = hs5.get_rss_mb()
    with hs5.track_rss(interval=0.1) as tracker:
        run_registry_bench(benchmark, server_name, s3_target(hs5))
    rss_after = hs5.get_rss_mb()
    print(
        f"\nHS5 RAM usage: before={rss_before:.1f} MB, peak={tracker.peak_mb:.1f} MB, after={rss_after:.1f} MB, delta={rss_after - rss_before:.1f} MB"
    )


def test_perf_docker_registry_hs5(benchmark: BenchmarkFixture, hs5_perf: Hs5Runner):
    """
    Push and pull Docker images via a registry storing into HS5
    """
    run_hs5_registry_bench(benchmark, "hs5", hs5_perf)


def test_perf_docker_registry_hs5_no_wal(
    benchmark: BenchmarkFixture, hs5_perf_no_wal: Hs5Runner
):
    """
    Push and pull Docker images via a registry storing into HS5 without index WAL
    """
    run_hs5_registry_bench(benchmark, "hs5_no_wal", hs5_perf_no_wal)


def test_perf_docker_registry_minio(benchmark: BenchmarkFixture, minio: MinioRunner):
    """
    Push and pull Docker images via a registry storing into MinIO
    """
    run_registry_bench(benchmark, "minio", s3_target(minio))


@pytest.mark.skipif(
    os.getenv("RUSTFS_ENABLED") != "1",
    reason="RUSTFS_ENABLED environment variable not set to 1",
)
def test_perf_docker_registry_rustfs(benchmark: BenchmarkFixture, rustfs: RustfsRunner):
    """
    Push and pull Docker images via a registry storing into RustFS
    """
    run_registry_bench(benchmark, "rustfs", s3_target(rustfs))
