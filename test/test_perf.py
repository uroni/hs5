
import contextvars
from dataclasses import dataclass
import io
import multiprocessing
import os
from pathlib import Path
import subprocess
import threading
import time
from typing import Optional
import uuid
from concurrent.futures import ThreadPoolExecutor

from mypy_boto3_s3 import S3Client
import pytest
import requests

from minio_fixture import calculate_sha256, minio, MinioRunner
from hs5_fixture import hs5_perf, Hs5Runner
from rustfs_fixture import rustfs, RustfsRunner
from garage_fixture import garage, GarageRunner, garage_sqlite, garage_sqlite_full
from pytest_benchmark.fixture import BenchmarkFixture
from multiprocessing import Pool

def prep_warp():
    def _wap_url() -> tuple[str, str]:
        # if arm64
        if os.uname().machine == "aarch64":
            return ("https://dl.min.io/aistor/warp/release/linux-arm64/archive/warp.v1.4.1",
                    "06ca770dd68f7a9fa736ebaeffd120ebd80210fd80cfff3c4972e232ad4ec6af")
        else:
            return ("https://dl.min.io/aistor/warp/release/linux-amd64/archive/warp.v1.4.1",
                    "62b59ad40e609462ec769bc2d86a8552590dcdebd0e73dbeca5b8813a166f937")
        
    url, expected_sha256 = _wap_url()
    loc = Path("warp")
    loc_new = Path("warp.new")
    if not loc.exists():
        print("Downloading Warp client...")
        with requests.get(url, stream=True) as r:
            r.raise_for_status()
            with open(loc_new, "wb") as f:
                for chunk in r.iter_content(chunk_size=8192):
                    f.write(chunk)
        sha256 = calculate_sha256(loc_new)
        if sha256 != expected_sha256:
            raise ValueError(f"Downloaded Warp client has invalid SHA-256 checksum: expected {expected_sha256}, got {sha256}")
        os.chmod(loc_new, 0o755)
        loc_new.rename(loc)

@dataclass
class WarpConfig:
    benchmark: str
    duration: str
    concurrent: int
    objects: int
    
def run_warp(server_url: str, access_key: str, secret_key: str, bucketname: str, out_fn: str, warp_config: WarpConfig):
    if server_url.startswith("http://"):
        server_url = server_url[len("http://"):]

    cfg = f"""warp:
  api: v1
  benchmark: {warp_config.benchmark}
  host: {server_url}
  access-key: {access_key}
  secret-key: {secret_key}
  bucket: {bucketname}
  duration: {warp_config.duration}
  concurrent: {warp_config.concurrent}
  objects: {warp_config.objects}
  analyze:
    dur: 30s
    verbose: true
    skip-duration: 10s
  benchdata: {out_fn}
"""
    with open("warp_config.yaml", "w") as f:
        f.write(cfg)

    subprocess.run(["./warp", "run", "warp_config.yaml"], check=True, shell=False)

def warp_cmp(benchmark: str, before_fn: Path, after_fn: Path, out_fn: Path):
    with open(out_fn, "w") as f:
        subprocess.run(["./warp", "cmp", str(before_fn), str(after_fn)], check=True, shell=False, stdout=f)
    with open(out_fn, "r") as f:
        out = f.read()
    with open(out_fn, "w") as f:
        f.write(f"Warp comparison benchmark={benchmark} between {before_fn} (before) and {after_fn} (after)\n\n")
        f.write(out)

warp_prev_result_fn : Optional[Path] = None
warp_prev_target : Optional[str] = None

#@pytest.mark.skipif(os.getenv("MINIO_ENABLED") != "1", reason="MINIO_ENABLED environment variable not set to 1")
@pytest.mark.parametrize("target", ["hs5", "minio"])
def test_perf_warp_mixed(benchmark: BenchmarkFixture, target: str, hs5_perf: Hs5Runner, minio: MinioRunner, tmp_path: Path):
    """
    Run Warp against HS5 and MinIO and record each run in pytest-benchmark.
    """
    prep_warp()
    bench = "mixed"
    warp_config = WarpConfig(benchmark=bench, duration="30s", concurrent=30, objects=100)

    if target == "hs5":
        server = hs5_perf
    else:
        server = minio

    out_path = tmp_path / f"warp-{target}"
    out_path_final = out_path.with_suffix(".json.zst")
    benchmark.extra_info["warp_target"] = target
    benchmark.extra_info["warp_output"] = str(out_path)

    benchmark.pedantic(
        run_warp,
        args=(
            server.get_url(),
            server.get_root_user(),
            server.get_root_key(),
            server.testbucketname(),
            str(out_path),
            warp_config,
        ),
        rounds=1,
        iterations=1,
        warmup_rounds=0,
    )

    time.sleep(1)

    global warp_prev_result_fn
    global warp_prev_target

    if warp_prev_result_fn:
        warp_cmp(bench, warp_prev_result_fn, out_path_final, Path(f"{bench}_{warp_prev_target}_vs_{target}.txt"))

    warp_prev_result_fn = out_path_final
    warp_prev_target = target


@pytest.mark.skipif(os.getenv("MINIO_ENABLED") != "1", reason="MINIO_ENABLED environment variable not set to 1")
def test_put_get_minio(tmp_path: Path, minio: MinioRunner):
    """
    Make sure the minio fixture is working
    """

    fdata = os.urandom(29*1024*1024)
    with open(tmp_path / "upload.txt", "wb") as upload_file:
        upload_file.write(fdata)
    
    s3_client = minio.get_s3_client()
    with io.FileIO(tmp_path / "upload.txt", "rb") as upload_file:
        s3_client.put_object(Bucket=minio.testbucketname(), Key="upload.txt", Body=upload_file)

    obj_info = s3_client.head_object(Bucket=minio.testbucketname(), Key="upload.txt")
    assert obj_info["ContentLength"] == len(fdata)

    obj_range = s3_client.get_object(Bucket=minio.testbucketname(), Key="upload.txt", Range="bytes=0-9")
    bdata = obj_range["Body"].read()
    assert len(bdata) == 10
    assert bdata == fdata[:10]


@pytest.mark.skipif(os.getenv("RUSTFS_ENABLED") != "1", reason="RUSTFS_ENABLED environment variable not set to 1")
def test_put_get_rustfs(tmp_path: Path, rustfs: RustfsRunner):
    """
    Make sure the rustfs fixture is working
    """

    fdata = os.urandom(29*1024*1024)
    with open(tmp_path / "upload.txt", "wb") as upload_file:
        upload_file.write(fdata)
    
    s3_client = rustfs.get_s3_client()
    with io.FileIO(tmp_path / "upload.txt", "rb") as upload_file:
        s3_client.put_object(Bucket=rustfs.testbucketname(), Key="upload.txt", Body=upload_file)

    obj_info = s3_client.head_object(Bucket=rustfs.testbucketname(), Key="upload.txt")
    assert obj_info["ContentLength"] == len(fdata)

    obj_range = s3_client.get_object(Bucket=rustfs.testbucketname(), Key="upload.txt", Range="bytes=0-9")
    bdata = obj_range["Body"].read()
    assert len(bdata) == 10
    assert bdata == fdata[:10]

@pytest.mark.skip(reason="Garage is slow to start up")
def test_put_get_garage(tmp_path: Path, garage: GarageRunner):
    """
    Make sure the garage fixture is working
    """

    fdata = os.urandom(29*1024*1024)
    with open(tmp_path / "upload.txt", "wb") as upload_file:
        upload_file.write(fdata)
    
    s3_client = garage.get_s3_client()
    with io.FileIO(tmp_path / "upload.txt", "rb") as upload_file:
        s3_client.put_object(Bucket=garage.testbucketname(), Key="upload.txt", Body=upload_file)

    obj_info = s3_client.head_object(Bucket=garage.testbucketname(), Key="upload.txt")
    assert obj_info["ContentLength"] == len(fdata)

    obj_range = s3_client.get_object(Bucket=garage.testbucketname(), Key="upload.txt", Range="bytes=0-9")
    bdata = obj_range["Body"].read()
    assert len(bdata) == 10
    assert bdata == fdata[:10]

@dataclass
class FileUpload:
    tmp_path: Path
    s3_client: S3Client
    bname: str

file_upload : Optional[FileUpload] = None

def upload_file(file_idx: int):
    assert file_upload is not None
    fpath = file_upload.tmp_path / f"test_file_{file_idx}.txt"
    with open(fpath, "wb") as f:
        f.write(b"a" * 1024) 
    file_upload.s3_client.upload_file(str(fpath), file_upload.bname, fpath.name)
    fpath.unlink()

def upload_many_files(get_s3_client, tmp_path: Path):
    """
    Upload 10,000 files to S3 server into a new bucket
    """
    bname = f"test-{uuid.uuid4()}"
    s3_client : S3Client = get_s3_client()
    s3_client.create_bucket(Bucket=bname)

    num_files = 10000
    chunk_size = num_files//multiprocessing.cpu_count()

    def init_file_upload():
        global file_upload
        file_upload = FileUpload(tmp_path, get_s3_client(), bname)

    with Pool(multiprocessing.cpu_count(), initializer=init_file_upload) as pool:
        pool.map(upload_file, range(0, num_files), chunk_size)

def test_many_range_downloads(hs5_perf: Hs5Runner, tmp_path: Path):
    """
    Test many range downloads from HS5.
    """
    s3_client = hs5_perf.get_s3_client()
    bname = hs5_perf.testbucketname()

    fsize = 50*1024*1024
    fdata = os.urandom(fsize)
    with open(tmp_path / "large_file.txt", "wb") as upload_file:
        upload_file.write(fdata)
    
    with io.FileIO(tmp_path / "large_file.txt", "rb") as upload_file:
        s3_client.put_object(Bucket=bname, Key="large_file.txt", Body=upload_file)

    def download_range(range_header: str):
        obj_range = s3_client.get_object(Bucket=bname, Key="large_file.txt", Range=range_header)
        bdata = obj_range["Body"].read()
        return bdata

    num_downloads = 1000
    range_headers = []
    for i in range(num_downloads):
        start = (i * 1024) % fsize
        end = start + 1023
        range_header = f"bytes={start}-{end}"
        range_headers.append(range_header)

    # Use thread pool with maximum 10 threads
    with ThreadPoolExecutor(max_workers=10) as executor:
        futures = [executor.submit(download_range, range_header) for range_header in range_headers]
        # Wait for all downloads to complete
        for future in futures:
            future.result()

def test_perf_upload_many_files_hs5(benchmark: BenchmarkFixture, hs5_perf: Hs5Runner, tmp_path: Path):
    """
    Test the performance of uploading 10,000 files to HS5.
    """
    rss_before = hs5_perf.get_rss_mb()
    with hs5_perf.track_rss(interval=0.1) as tracker:
        benchmark(upload_many_files, lambda: hs5_perf.get_s3_client(), tmp_path)
    rss_after = hs5_perf.get_rss_mb()
    print(f"\nHS5 RAM usage: before={rss_before:.1f} MB, peak={tracker.peak_mb:.1f} MB, after={rss_after:.1f} MB, delta={rss_after - rss_before:.1f} MB")

def test_perf_upload_many_files_minio(benchmark: BenchmarkFixture, minio: MinioRunner, tmp_path: Path):
    """
    Test the performance of uploading 10,000 files to MinIO.
    """
    benchmark(upload_many_files, lambda: minio.get_s3_client(), tmp_path)

@pytest.mark.skipif(os.getenv("RUSTFS_ENABLED") != "1", reason="RUSTFS_ENABLED environment variable not set to 1")
def test_perf_upload_many_files_rustfs(benchmark: BenchmarkFixture, rustfs: RustfsRunner, tmp_path: Path):
    """
    Test the performance of uploading 10,000 files to RustFS.
    """
    benchmark(upload_many_files, lambda: rustfs.get_s3_client(), tmp_path)


def test_perf_upload_many_files_garage_sqlite(benchmark: BenchmarkFixture, garage_sqlite: GarageRunner, tmp_path: Path):
    """
    Test the performance of uploading 10,000 files to Garage with SQLite.
    """
    benchmark(upload_many_files, lambda: garage_sqlite.get_s3_client(), tmp_path)

@pytest.mark.skip(reason="Garage needs to be re-compiled with sqlite synchronous set to FULL for this test")
def test_perf_upload_many_files_garage_sqlite_full(benchmark: BenchmarkFixture, garage_sqlite_full: GarageRunner, tmp_path: Path):
    """
    Test the performance of uploading 10,000 files to Garage with SQLite Full.
    """
    benchmark(upload_many_files, lambda: garage_sqlite_full.get_s3_client(), tmp_path)
