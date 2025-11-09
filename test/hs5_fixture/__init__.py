# Copyright Martin Raiber. All Rights Reserved.
# SPDX-License-Identifier: LGPL-3.0-or-later
from dataclasses import dataclass
from pathlib import Path
from shutil import rmtree
import subprocess
import sys
import time
from typing import Optional
import uuid
import botocore
import botocore.config
import pytest
import os
import boto3
from mypy_boto3_s3.client import S3Client
import requests

curr_port = 11000

@dataclass
class Hs5Stats:
    used: int
    size: int
    free_space: int

    def __str__(self) -> str:
        return f"Used: {self.used}, Size: {self.size}, Free space: {self.free_space}"


class Hs5Runner:
    manual_commit = False
    with_heaptrack = False

    def __init__(self, workdir : Path, data_file_size_limit_mb: int, perf: bool = False, data_file_alloc_chunk_size: Optional[int] = None) -> None:
        global curr_port

        curr_port += 1

        self._port = curr_port
        self._workdir = workdir
        self._root_key = uuid.uuid4().hex

        if data_file_alloc_chunk_size is None:
            data_file_alloc_chunk_size_str = os.getenv("DATA_FILE_ALLOC_CHUNKSIZE")
            if data_file_alloc_chunk_size_str is None:
                data_file_alloc_chunk_size = 10*1024*1024
            else:
                data_file_alloc_chunk_size = int(data_file_alloc_chunk_size_str)

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

        self.args = [str(hs5_loc),
                "internal",
                "--ip",
                "127.0.0.1",
                "--http_port",
            str(curr_port),
            "--init_root_password", self._root_key,
            "--with_stop_command", "true",
            "--bucket_versioning=false"]

        if os.environ.get("MANUAL_COMMIT") == "1":
            self.manual_commit = True
        
        if os.environ.get("ENABLE_WAL") == "1":
            self.args.append("--index_wal_path")
            self.args.append(".")

        if os.environ.get("ENABLE_WAL_WRITE_DATA") == "1":
            self.args.append("--wal_write_data")

        if os.environ.get("ENABLE_WAL_WRITE_META") == "0":
            self.args.append("--nowal_write_meta")

        if perf:
            self.args.append("--logging")
            self.args.append("WARN")
        else:
            self.args.append("--data_file_size_limit_mb")
            self.args.append(str(data_file_size_limit_mb))
            self.args.append("--data_file_alloc_chunk_size")
            self.args.append(str(data_file_alloc_chunk_size))
            self.args.append("--logging")
            self.args.append("INFO")

        if self.manual_commit:
            self.args.append("--manual_commit")

        if self.with_heaptrack:
            self.args.insert(0, "heaptrack")

        self.args.append("--check_freespace_on_startup")

        if not perf:
            self.args.append("--wal_write_delay")

        self.args.append("--enable_core_dumps")

        self.start()

        self._login_admin()

        self.get_s3_client().create_bucket(Bucket=self.testbucketname())

    def start(self) -> None:
        self._process = subprocess.Popen(
            self.args,
            stdout=sys.stdout,
            stderr=sys.stderr,
            cwd=self._workdir
        )

        self._wait_for_startup()

    def _wait_for_startup(self) -> None:
        while True:
            try:
                resp = requests.get(self.get_url(), timeout=0.5)
                break
            except:
                time.sleep(0.01)

    def _login_admin(self) -> None:

        self._ses = requests.session()

        response = self._ses.post(self.get_api_url()+"login", json={
            "username": "root",
            "password": self._root_key
        })

        assert response.status_code == 200
        resp = response.json()
        self._admin_ses = resp["ses"]
        assert self._admin_ses

    def stop(self, cleanup: bool) -> None:

        with pytest.raises(subprocess.TimeoutExpired):
            self._process.wait(0.001)

        if self.with_heaptrack:
            self.stop_server()
        else:
            self._process.kill()

        self._process.wait()

        if not self.with_heaptrack and cleanup:
            rmtree(self._workdir)

    def restart(self) -> None:
        self.stop(cleanup=False)
        self.start()

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
    
    def get_datafile_size(self) -> int:
        data_file_path = self._workdir / "data0"
        if not data_file_path.exists():
            return 0
        return os.path.getsize(data_file_path)
    
    def get_stats(self) -> Hs5Stats:
        url = self.get_api_url() + "stats"
        response = self._ses.post(url, json={"ses": self._admin_ses})
        
        assert response.status_code == 200
        resp = response.json()
        return Hs5Stats(**resp)
    

@pytest.fixture
def hs5(tmpdir: Path):
    loc = tmpdir / uuid.uuid4().hex
    loc.mkdir()
    runner = Hs5Runner(loc, data_file_size_limit_mb=100)
    yield runner
    runner.stop(cleanup=True)
    try:
        rmtree(loc)
    except:
        pass

@pytest.fixture
def hs5_perf(tmpdir: Path):
    loc = tmpdir / uuid.uuid4().hex
    loc.mkdir()
    runner = Hs5Runner(loc, data_file_size_limit_mb=100, perf=True)
    yield runner
    runner.stop(cleanup=True)
    try:
        rmtree(loc)
    except:
        pass

@pytest.fixture
def hs5_large(tmpdir: Path):
    loc = tmpdir / uuid.uuid4().hex
    loc.mkdir()
    runner = Hs5Runner(loc, data_file_size_limit_mb=5000)
    yield runner
    runner.stop(cleanup=True)
    try:
        rmtree(loc)
    except:
        pass

@pytest.fixture
def hs5_large_small_alloc_chunksize(tmpdir: Path):
    loc = tmpdir / uuid.uuid4().hex
    loc.mkdir()
    runner = Hs5Runner(loc, data_file_size_limit_mb=5000, data_file_alloc_chunk_size=10*1024*1024)
    yield runner
    runner.stop(cleanup=True)
    try:
        rmtree(loc)
    except:
        pass