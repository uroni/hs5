# Copyright Martin Raiber. All Rights Reserved.
# SPDX-License-Identifier: LGPL-3.0-or-later
from pathlib import Path
from shutil import rmtree
import subprocess
import sys
import uuid
import botocore
import pytest
import os
import boto3
from mypy_boto3_s3.client import S3Client

curr_port = 11000

class Hs5Runner:
    manual_commit = False

    def __init__(self, workdir : Path, data_file_size_limit_mb: int) -> None:
        global curr_port

        curr_port += 1

        self._port = curr_port
        self._workdir = workdir
        self._root_key = uuid.uuid4().hex

        data_file_alloc_chunk_size = os.getenv("DATA_FILE_ALLOC_CHUNKSIZE")
        if data_file_alloc_chunk_size is None:
            data_file_alloc_chunk_size = str(10*1024*1024)

        args = [f"{os.getcwd()}/build/hs5",
                "--ip",
                "127.0.0.1",
                "--http_port",
            str(curr_port),
            "--root_key",
            self._root_key,
            "--data_file_size_limit_mb",
            str(data_file_size_limit_mb),
            "--data_file_alloc_chunk_size",
            data_file_alloc_chunk_size,
            "--logging", "DBG0"]
        
        if self.manual_commit:
            args.append("--manual_commit")

        self._process = subprocess.Popen(
            args,
            stdout=sys.stdout,
            stderr=sys.stderr,
            cwd=workdir
        )
        pass

    def stop(self) -> None:

        with pytest.raises(subprocess.TimeoutExpired):
            self._process.wait(0.001)

        self._process.kill()
        self._process.wait()

        rmtree(self._workdir)

    def get_url(self) -> str:
        return f"http://127.0.0.1:{self._port}"

    def get_s3_client(self) -> S3Client:
        return boto3.client('s3', endpoint_url=self.get_url(), aws_access_key_id="root", aws_secret_access_key=self._root_key)
    
    def commit_storage(self, s3: S3Client):
        if not self.manual_commit:
            return

        s3.put_object(Bucket="foo", Key="a711e93e-93b4-4a9e-8a0b-688797470002", Body="")



@pytest.fixture
def hs5(tmpdir: Path):
    runner = Hs5Runner(tmpdir, data_file_size_limit_mb=100)
    yield runner
    runner.stop()

@pytest.fixture
def hs5_large(tmpdir: Path):
    runner = Hs5Runner(tmpdir, data_file_size_limit_mb=5000)
    yield runner
    runner.stop()