# Copyright Martin Raiber. All Rights Reserved.
# SPDX-License-Identifier: LGPL-3.0-or-later
from pathlib import Path
from shutil import rmtree
import subprocess
import sys
import time
import uuid
import botocore
import botocore.config
import pytest
import os
import boto3
from mypy_boto3_s3.client import S3Client
import requests

curr_port = 11000

class Hs5Runner:
    manual_commit = False
    with_heaptrack = False

    def __init__(self, workdir : Path, data_file_size_limit_mb: int) -> None:
        global curr_port

        curr_port += 1

        self._port = curr_port
        self._workdir = workdir
        self._root_key = uuid.uuid4().hex

        data_file_alloc_chunk_size = os.getenv("DATA_FILE_ALLOC_CHUNKSIZE")
        if data_file_alloc_chunk_size is None:
            data_file_alloc_chunk_size = str(10*1024*1024)

        locs = [Path("builds/ninja-multi-vcpkg/Debug/hs5"),
                Path("builds/ninja-multi-vcpkg/RelWithDebInfo/hs5"),
                Path("builds/ninja-multi-vcpkg/Release/hs5"),
                Path("build/hs5")]
        
        hs5_loc = None
        for loc in locs:
            if (Path(os.getcwd()) / loc).exists():
                hs5_loc = Path(os.getcwd()) / loc
                break

        assert hs5_loc is not None

        args = [str(hs5_loc),
                "internal",
                "--ip",
                "127.0.0.1",
                "--http_port",
            str(curr_port),
            "--data_file_size_limit_mb",
            str(data_file_size_limit_mb),
            "--data_file_alloc_chunk_size",
            data_file_alloc_chunk_size,
            "--logging", "DBG0",
            "--init_root_password", self._root_key,
            "--with_stop_command", "true"]
        
        if self.manual_commit:
            args.append("--manual_commit")

        if self.with_heaptrack:
            args.insert(0, "heaptrack")

        self._process = subprocess.Popen(
            args,
            stdout=sys.stdout,
            stderr=sys.stderr,
            cwd=workdir
        )

        self._wait_for_startup()

        self.get_s3_client().create_bucket(Bucket=self.testbucketname())

    def _wait_for_startup(self) -> None:
        while True:
            try:
                resp = requests.get(self.get_url(), timeout=0.5)
                break
            except:
                time.sleep(0.01)

    def stop(self) -> None:

        with pytest.raises(subprocess.TimeoutExpired):
            self._process.wait(0.001)

        if self.with_heaptrack:
            self.stop_server()
        else:
            self._process.kill()

        self._process.wait()

        if not self.with_heaptrack:
            rmtree(self._workdir)

    def get_url(self) -> str:
        return f"http://127.0.0.1:{self._port}"
    
    def get_api_url(self) -> str:
        return f"http://127.0.0.1:{self._port}/api-v1-b64be512-4b03-4028-a589-13931942e205/"

    def get_s3_client(self, sig_v2: bool = False) -> S3Client:
        config = botocore.config.Config(signature_version='s3v4') if not sig_v2 else None
        return boto3.client('s3', endpoint_url=self.get_url(), aws_access_key_id="root", aws_secret_access_key=self._root_key, config=config)
    
    def commit_storage(self, s3: S3Client):
        if not self.manual_commit:
            return

        s3.put_object(Bucket="foo", Key="a711e93e-93b4-4a9e-8a0b-688797470002", Body="")

    def stop_server(self):
        s3 = self.get_s3_client()
        try:
            s3.put_object(Bucket="foo", Key="3db7da22-8ce2-4420-a8ca-f09f0b8e0e61", Body="")
        except:
            pass

    def get_root_key(self):
        return self._root_key
    
    def testbucketname(self):
        return "testbucket"

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