# Copyright Martin Raiber. All Rights Reserved.
# SPDX-License-Identifier: LGPL-3.0-or-later
"""
Litestream (SQLite -> S3 replication) benchmark.

Each commit to a local SQLite database is followed by a forced Litestream
sync that waits until the transaction is stored in S3 ("synchronous
replication"). The commit -> durable-in-S3 latency is therefore dominated by
the S3 server's (small object) PUT latency. Afterwards the database is
restored from S3 and verified.
"""
from dataclasses import dataclass
import http.client
import json
import os
from pathlib import Path
import signal
import socket
import sqlite3
import statistics
import subprocess
import sys
import tarfile
import threading
import time
from typing import Optional

from mypy_boto3_s3 import S3Client
import portalocker
import pytest
import requests
from pytest_benchmark.fixture import BenchmarkFixture

from minio_fixture import calculate_sha256, minio
from hs5_fixture import hs5_perf
from rustfs_fixture import rustfs
from garage_fixture import garage_sqlite, GarageRunner

LITESTREAM_VERSION = "0.5.18"
LITESTREAM_SHA256 = {
    "x86_64": "d3687b9fd0f2e9426bbd3da828429ff3f1dfc5c9b74d54fdf028e4ee510e7dea",
    "arm64": "922a09e6a7ac67f2095571894dc6382e94e1dd4cdd9674883ddf1a8e74ab982d",
}

SERVERS = [
    # Baseline: replicate to a local directory instead of S3 (Litestream overhead)
    "local_file",
    "hs5_perf",
    "minio",
    "garage_sqlite",
    pytest.param(
        "rustfs",
        marks=pytest.mark.skipif(
            os.getenv("RUSTFS_ENABLED") != "1",
            reason="RUSTFS_ENABLED environment variable not set to 1",
        ),
    ),
]


def prep_litestream() -> Path:
    arch = "arm64" if os.uname().machine == "aarch64" else "x86_64"
    url = (
        f"https://github.com/benbjohnson/litestream/releases/download/v{LITESTREAM_VERSION}/"
        f"litestream-{LITESTREAM_VERSION}-linux-{arch}.tar.gz"
    )
    loc = Path("litestream")
    with portalocker.Lock("litestream.download.lock", timeout=600):
        if loc.exists():
            return loc.absolute()

        print("Downloading Litestream...")
        tgz = Path("litestream.tar.gz.new")
        with requests.get(url, stream=True) as r:
            r.raise_for_status()
            with open(tgz, "wb") as f:
                for chunk in r.iter_content(chunk_size=65536):
                    f.write(chunk)
        sha256 = calculate_sha256(tgz)
        if sha256 != LITESTREAM_SHA256[arch]:
            tgz.unlink()
            raise ValueError(
                f"Downloaded Litestream has invalid SHA-256 checksum: expected {LITESTREAM_SHA256[arch]}, got {sha256}"
            )
        loc_new = Path("litestream.new")
        with tarfile.open(tgz) as tf:
            member = tf.getmember("litestream")
            src = tf.extractfile(member)
            assert src is not None
            with open(loc_new, "wb") as f:
                f.write(src.read())
        tgz.unlink()
        loc_new.chmod(0o755)
        loc_new.rename(loc)
    return loc.absolute()


class _UnixHTTPConnection(http.client.HTTPConnection):
    def __init__(self, socket_path: str):
        super().__init__("localhost", timeout=60)
        self._socket_path = socket_path

    def connect(self) -> None:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(self.timeout)
        s.connect(self._socket_path)
        self.sock = s


@dataclass
class S3Target:
    endpoint: str
    region: str
    access_key: str
    secret_key: str
    bucket: str


def s3_target(server) -> S3Target:
    s3: S3Client = server.get_s3_client()
    creds = s3._request_signer._credentials  # type: ignore[attr-defined]
    return S3Target(
        endpoint=s3.meta.endpoint_url,
        # Garage only accepts its configured region
        region=(
            "garage"
            if isinstance(server, GarageRunner)
            else s3.meta.region_name or "us-east-1"
        ),
        access_key=creds.access_key,
        secret_key=creds.secret_key,
        bucket=server.testbucketname(),
    )


class Litestream:
    def __init__(self, workdir: Path, target: Optional[S3Target], db_paths: list[Path]):
        self._bin = prep_litestream()
        self.socket_path = str(workdir / "litestream.sock")
        self.config_path = workdir / "litestream.yml"
        # JSON is valid YAML, so no YAML dependency is needed
        config = {
            "socket": {"enabled": True, "path": self.socket_path},
            "logging": {"level": "warn"},
            "dbs": [
                {
                    "path": str(p),
                    "replica": (
                        {"type": "file", "path": str(workdir / "replica" / p.stem)}
                        if target is None
                        else {
                            "type": "s3",
                            "bucket": target.bucket,
                            "path": p.stem,
                            "endpoint": target.endpoint,
                            "region": target.region,
                            "force-path-style": True,
                            "access-key-id": target.access_key,
                            "secret-access-key": target.secret_key,
                        }
                    ),
                }
                for p in db_paths
            ],
        }
        self.config_path.write_text(json.dumps(config))
        self._process = subprocess.Popen(
            [str(self._bin), "replicate", "-config", str(self.config_path)],
            stdout=sys.stdout,
            stderr=sys.stderr,
        )
        for p in db_paths:
            self._wait_open(p)

    def _wait_open(self, db_path: Path) -> None:
        start = time.monotonic()
        while True:
            assert self._process.poll() is None, "litestream exited"
            try:
                status, body = self.sync(_UnixHTTPConnection(self.socket_path), db_path)
                if status == 200:
                    return
                # 404/409: database not registered/opened yet
                assert status in (404, 409), body
            except (FileNotFoundError, ConnectionRefusedError):
                pass
            if time.monotonic() - start > 30:
                raise TimeoutError(f"Litestream did not open {db_path}")
            time.sleep(0.05)

    def connect(self) -> _UnixHTTPConnection:
        return _UnixHTTPConnection(self.socket_path)

    @staticmethod
    def sync(conn: _UnixHTTPConnection, db_path: Path) -> tuple[int, dict]:
        conn.request(
            "POST",
            "/sync",
            body=json.dumps({"path": str(db_path), "wait": True}),
            headers={"Content-Type": "application/json"},
        )
        resp = conn.getresponse()
        return resp.status, json.loads(resp.read())

    def restore(self, db_path: Path, out_path: Path) -> float:
        start = time.monotonic()
        subprocess.run(
            [
                str(self._bin),
                "restore",
                "-config",
                str(self.config_path),
                "-o",
                str(out_path),
                str(db_path),
            ],
            check=True,
        )
        return time.monotonic() - start

    def stop(self) -> None:
        if self._process.poll() is None:
            self._process.send_signal(signal.SIGTERM)
            try:
                self._process.wait(60)
            except subprocess.TimeoutExpired:
                self._process.kill()
                self._process.wait()


def create_db(path: Path) -> None:
    db = sqlite3.connect(path)
    db.execute("PRAGMA journal_mode=WAL")
    db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY, v BLOB)")
    db.commit()
    db.close()


def commit_and_replicate(
    ls: Litestream, db_path: Path, commits: int, row_size: int
) -> list[float]:
    """
    Do `commits` transactions, each followed by a sync that waits until the
    transaction is in S3. Returns per-commit latencies (commit + replicate).
    """
    db = sqlite3.connect(db_path, isolation_level=None, timeout=30)
    db.execute("PRAGMA busy_timeout=30000")
    conn = ls.connect()
    latencies = []
    try:
        for _ in range(commits):
            start = time.perf_counter()
            db.execute("INSERT INTO t (v) VALUES (randomblob(?))", (row_size,))
            status, body = ls.sync(conn, db_path)
            latencies.append(time.perf_counter() - start)
            assert status == 200, body
            # "no_change" if Litestream's background monitor already synced
            # the transaction; either way it must be replicated now
            assert body["status"] in ("synced", "no_change"), body
            assert body["replicated_txid"] >= body["txid"], body
    finally:
        conn.close()
        db.close()
    return latencies


def pct(sorted_vals: list[float], p: float) -> float:
    idx = min(len(sorted_vals) - 1, int(round(p / 100 * (len(sorted_vals) - 1))))
    return sorted_vals[idx]


def run_litestream_bench(
    benchmark: BenchmarkFixture,
    server_name: str,
    server,
    workdir: Path,
    num_dbs: int,
    commits_per_db: int,
    row_size: int = 1024,
) -> None:
    db_paths = [workdir / f"db{i}.sqlite" for i in range(num_dbs)]
    for p in db_paths:
        create_db(p)

    ls = Litestream(workdir, None if server is None else s3_target(server), db_paths)
    latencies: list[float] = []
    wall = 0.0
    try:

        def run():
            nonlocal wall
            results: list[list[float]] = [[] for _ in db_paths]
            errors: list[BaseException] = []

            def worker(i: int):
                try:
                    results[i] = commit_and_replicate(
                        ls, db_paths[i], commits_per_db, row_size
                    )
                except BaseException as e:
                    errors.append(e)

            threads = [
                threading.Thread(target=worker, args=(i,)) for i in range(num_dbs)
            ]
            start = time.perf_counter()
            for t in threads:
                t.start()
            for t in threads:
                t.join()
            wall = time.perf_counter() - start
            if errors:
                raise errors[0]
            for r in results:
                latencies.extend(r)

        benchmark.pedantic(run, rounds=1, iterations=1)

        # Verify replicated data by restoring the first database from S3
        restored = workdir / "restored.sqlite"
        restore_s = ls.restore(db_paths[0], restored)
        rdb = sqlite3.connect(restored)
        (rows,) = rdb.execute("SELECT COUNT(*) FROM t").fetchone()
        rdb.close()
        assert rows == commits_per_db
    finally:
        ls.stop()

    s = sorted(latencies)
    stats = {
        "commits": len(s),
        "commits_per_s": len(s) / wall,
        "p50_ms": pct(s, 50) * 1000,
        "p90_ms": pct(s, 90) * 1000,
        "p99_ms": pct(s, 99) * 1000,
        "max_ms": s[-1] * 1000,
        "mean_ms": statistics.fmean(s) * 1000,
        "restore_s": restore_s,
    }
    benchmark.extra_info.update(stats)
    print(
        f"\nLITESTREAM {server_name} dbs={num_dbs} commits={stats['commits']} "
        f"rate={stats['commits_per_s']:.1f}/s p50={stats['p50_ms']:.2f}ms "
        f"p90={stats['p90_ms']:.2f}ms p99={stats['p99_ms']:.2f}ms "
        f"max={stats['max_ms']:.2f}ms restore={restore_s:.2f}s"
    )


@pytest.mark.parametrize("server_name", SERVERS)
def test_perf_litestream_sync(
    benchmark: BenchmarkFixture,
    server_name: str,
    request: pytest.FixtureRequest,
    tmp_path: Path,
):
    """
    One SQLite database, every commit waits until it is replicated to S3.
    """
    server = (
        None if server_name == "local_file" else request.getfixturevalue(server_name)
    )
    commits = int(os.getenv("LITESTREAM_COMMITS", "1000"))
    run_litestream_bench(benchmark, server_name, server, tmp_path, 1, commits)


@pytest.mark.parametrize("server_name", SERVERS)
def test_perf_litestream_sync_multi_db(
    benchmark: BenchmarkFixture,
    server_name: str,
    request: pytest.FixtureRequest,
    tmp_path: Path,
):
    """
    Many SQLite databases (e.g. one per tenant) replicating concurrently, every
    commit waits until it is replicated to S3.
    """
    server = (
        None if server_name == "local_file" else request.getfixturevalue(server_name)
    )
    num_dbs = int(os.getenv("LITESTREAM_DBS", "16"))
    commits = int(os.getenv("LITESTREAM_COMMITS_PER_DB", "200"))
    run_litestream_bench(benchmark, server_name, server, tmp_path, num_dbs, commits)
