# Copyright Martin Raiber. All Rights Reserved.
# SPDX-License-Identifier: AGPL-3.0-or-later

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

    def __init__(self, workdir : Path) -> None:
        global curr_port

        curr_port += 1

        self._port = curr_port
        self._workdir = workdir
        self._root_key = uuid.uuid4().hex

        self._process = subprocess.Popen(
            [f"{os.getcwd()}/build/hs5",
                "-http_port",
            str(curr_port),
            "-root_key",
            self._root_key,
            "-data_file_size_limit_mb",
            "100",
            "-data_file_alloc_chunk_size_mb",
            "10",
            "-logging", "DBG0"],
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



@pytest.fixture
def hs5(tmpdir: Path):
    runner = Hs5Runner(tmpdir)
    yield runner
    runner.stop()