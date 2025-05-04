# Copyright Martin Raiber. All Rights Reserved.
# SPDX-License-Identifier: LGPL-3.0-or-later
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import time
import uuid
import boto3
import botocore
import pytest
import requests
from mypy_boto3_s3.client import S3Client

curr_port = 12000

def calculate_sha256(file_path: Path) -> str:
    """
    Calculates the SHA-256 checksum of a file.

    :param file_path: Path to the file
    :return: SHA-256 checksum as a hexadecimal string
    """
    sha256_hash = hashlib.sha256()
    with open(file_path, "rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            sha256_hash.update(chunk)
    return sha256_hash.hexdigest().lower()

class MinioRunner:
    """
    A class that provides a fixture for running a MinIO server in a test environment.
    """

    def _download_minio(self):
        loc = Path("minio")
        loc_new = Path("minio.new")
        if not loc.exists():  
            print("Downloading MinIO server...")          
            url = "https://dl.min.io/server/minio/release/linux-amd64/archive/minio.RELEASE.2025-04-22T22-12-26Z"
            with requests.get(url, stream=True) as r:
                r.raise_for_status()
                with open(loc_new, 'wb') as f:
                    for chunk in r.iter_content(chunk_size=8192):
                        f.write(chunk)

            if calculate_sha256(loc_new) != "53e2a2cb16c5366ea6fbbc479c19ddb4c6a0948273e752f740fb1fbf27bb817c":
                raise ValueError("SHA256 checksum does not match expected value.")
            
            loc_new.rename(loc)

            # Enable execute permissions
            loc.chmod(0o755)                       


    def __init__(self, datapath : Path):
        """
        Initializes the MinioRunner 
        """
        global curr_port

        curr_port += 2

        self._port = curr_port
        self._root_key = uuid.uuid4().hex

        self._download_minio()

        nenv = os.environ.copy()
        nenv["MINIO_ROOT_USER"] = "root"
        nenv["MINIO_ROOT_PASSWORD"] = self._root_key

        self._process = subprocess.Popen(
            ["./minio", "server", "--address", f"localhost:{self._port}", "--console-address",  f"localhost:{self._port + 1}", 
             datapath], env=nenv,
             stdout=sys.stdout,
            stderr=sys.stderr,
        )

        self._wait_for_startup()

        self.get_s3_client().create_bucket(Bucket=self.testbucketname())

    def stop(self):
        """
        Stops the MinIO server
        """
        self._process.kill()
        self._process.wait()

    def testbucketname(self):
        return "testbucket"
    
    def get_url(self) -> str:
        return f"http://127.0.0.1:{self._port}"
    
    def _wait_for_startup(self) -> None:
        while True:
            try:
                resp = requests.get(self.get_url(), timeout=0.5)
                break
            except:
                time.sleep(0.01)

    def get_s3_client(self, sig_v2: bool = False) -> S3Client:
        return boto3.client('s3', endpoint_url=self.get_url(), aws_access_key_id="root", aws_secret_access_key=self._root_key)
    

@pytest.fixture
def minio(tmpdir: Path):
    runner = MinioRunner(tmpdir)
    yield runner
    runner.stop()
