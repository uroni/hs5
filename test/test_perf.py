
import contextvars
from dataclasses import dataclass
import io
import multiprocessing
import os
from pathlib import Path
import threading
from typing import Optional
import uuid

from mypy_boto3_s3 import S3Client

from minio_fixture import minio, MinioRunner
from hs5_fixture import hs5_perf, Hs5Runner
from pytest_benchmark.fixture import BenchmarkFixture
from multiprocessing import Pool


def test_put_get_minio(benchmark: BenchmarkFixture, tmp_path: Path, minio: MinioRunner):
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

def test_perf_upload_many_files_hs5(benchmark: BenchmarkFixture, hs5_perf: Hs5Runner, tmp_path: Path):
    """
    Test the performance of uploading 10,000 files to HS5.
    """
    benchmark(upload_many_files, lambda: hs5_perf.get_s3_client(), tmp_path)

def test_perf_upload_many_files_minio(benchmark: BenchmarkFixture, minio: MinioRunner, tmp_path: Path):
    """
    Test the performance of uploading 10,000 files to MinIO.
    """
    benchmark(upload_many_files, lambda: minio.get_s3_client(), tmp_path)