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

curr_port = 13000

class RustfsRunner:
    """
    A class that provides a fixture for running a RustFS server in a test environment.
    """

    def _download_rustfs(self):
        if os.getenv("RUSTFS_ENABLED") != "1":
            raise RuntimeError("RustFS tests are disabled. Set RUSTFS_ENABLED=1 to enable them.")

        loc = Path("rustfs")
        loc_new = Path("rustfs.new.zip")
        if not loc.exists():  
            print("Downloading RustFS server...")          
            url = "https://github.com/rustfs/rustfs/releases/download/1.0.0-alpha.83/rustfs-linux-x86_64-gnu-latest.zip"
            with requests.get(url, stream=True) as r:
                r.raise_for_status()
                with open(loc_new, 'wb') as f:
                    for chunk in r.iter_content(chunk_size=8192):
                        f.write(chunk)

            from zipfile import ZipFile
            with ZipFile(loc_new, 'r') as zip_ref:
                zip_ref.extractall(".")
            

            assert loc.is_file(), "Extracted RustFS binary not found."

            # Enable execute permissions
            loc.chmod(0o755)                       


    def __init__(self, datapath : Path):
        """
        Initializes the RustfsRunner 
        """
        global curr_port

        curr_port += 2

        self._port = curr_port
        self._root_key = uuid.uuid4().hex

        self._download_rustfs()

        nenv = os.environ.copy()
        nenv["RUSTFS_ACCESS_KEY"] = "root"
        nenv["RUSTFS_SECRET_KEY"] = self._root_key

        self._process = subprocess.Popen(
            ["./rustfs", datapath, "--address", f"localhost:{self._port}", "--console-address",  f"localhost:{self._port + 1}" ], env=nenv,
             stdout=sys.stdout,
            stderr=sys.stderr,
        )

        self._wait_for_startup()

        self.get_s3_client().create_bucket(Bucket=self.testbucketname())

    def stop(self):
        """
        Stops the RustFS server
        """
        self._process.kill()
        self._process.wait()

    def testbucketname(self):
        return "testbucket"
    
    def get_url(self) -> str:
        return f"http://localhost:{self._port}"
    
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
def rustfs(tmpdir: Path):
    runner = RustfsRunner(tmpdir)
    yield runner
    runner.stop()
