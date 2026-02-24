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
import sh

curr_port = 14000

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

class GarageRunner:
    """
    A class that provides a fixture for running a Garage server in a test environment.
    """

    def _download_garage(self):
        loc = Path("garage")
        loc_new = Path("garage.new")
        if not loc.exists():  
            print("Downloading Garage server...")          
            url = "https://garagehq.deuxfleurs.fr/_releases/v2.2.0/x86_64-unknown-linux-musl/garage"
            with requests.get(url, stream=True) as r:
                r.raise_for_status()
                with open(loc_new, 'wb') as f:
                    for chunk in r.iter_content(chunk_size=8192):
                        f.write(chunk)

            if calculate_sha256(loc_new) != "ec761bb996e8453e86fe68ccc1cf222c73bb1ef05ae0b540bd4827e7d1931aab":
                raise ValueError("SHA256 checksum does not match expected value.")
            
            loc_new.rename(loc)

            # Enable execute permissions
            loc.chmod(0o755)                       


    def __init__(self, datapath : Path, dbengine: str):
        """
        Initializes the GarageRunner 
        """
        global curr_port

        curr_port += 3

        self._port = curr_port
        self._root_key = uuid.uuid4().hex

        sqlite_full = dbengine == "sqlite_full"
        if sqlite_full:
            dbengine = "sqlite"

        self._download_garage()           

        self._config_path = datapath / "config.toml"

        meta_path = datapath / "meta"
        data_path = datapath / "data"

        meta_path.mkdir()
        data_path.mkdir()

        self._secret = uuid.uuid4().hex + uuid.uuid4().hex

        self._config_path.write_text(
f"""metadata_dir = "{meta_path}"
data_dir = "{data_path}"
db_engine = "{dbengine}"
replication_factor = 1
rpc_bind_addr = "[::]:{curr_port+1}"
rpc_public_addr = "127.0.0.1:{curr_port+1}"
rpc_secret = "{self._secret}"
data_fsync = true
metadata_fsync = true
compression_level = 'none'
[s3_api]
s3_region = "garage"
api_bind_addr = "[::]:{curr_port}"
[admin]
api_bind_addr = "[::]:{curr_port+2}"
admin_token = "{self._secret}"
metrics_token = "{self._secret}"
""", encoding="utf-8")
        
        executable = "./garage" if not sqlite_full else "./sqlite_full_garage"

        self._process = subprocess.Popen(
            [executable, "-c", str(self._config_path), "server"],
             stdout=sys.stdout,
            stderr=sys.stderr,
        )

        self._node_id = self.get_node_id()

        garage_bin = sh.Command("./garage")
        garage = garage_bin.bake("-c", str(self._config_path))
        garage("layout", "assign", "-z", "dc1", "-c", "50G", self._node_id)
        garage("layout", "apply", "--version", "1")

        garage("bucket", "create", self.testbucketname())
        key_info_lines = garage("key", "create", "root").splitlines() # type: ignore
        self._access_key = key_info_lines[1].split(": ", 1)[1].strip()
        self._secret_key = key_info_lines[3].split(": ", 1)[1].strip()

        garage("key", "allow", "--create-bucket", "root")
        garage("bucket", "allow", "--read", "--write", "--owner", self.testbucketname(), "--key", "root")

        self._wait_for_startup()

    def stop(self):
        """
        Stops the Garage server
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
        return boto3.client('s3', endpoint_url=self.get_url(), aws_access_key_id=self._access_key, aws_secret_access_key=self._secret_key)


    def get_node_id(self) -> str:
        garage = sh.Command("./garage")
        starttime = time.monotonic()
        while time.monotonic() - starttime < 5:
            try:
                for line in garage("-c", str(self._config_path), "status", _iter=True): # type: ignore
                    line : str
                    print(f"Got line: {line}")
                    if not line.startswith("==") and not line.startswith("ID"):
                        return line.split(" ", 1)[0].strip()
                    
            except:
                pass

            time.sleep(0.01)

        raise TimeoutError("Garage server did not start within 5 seconds")
    

@pytest.fixture
def garage(tmpdir: Path):
    runner = GarageRunner(tmpdir, "lmdb")
    yield runner
    runner.stop()

@pytest.fixture
def garage_sqlite(tmpdir: Path):
    runner = GarageRunner(tmpdir, "sqlite")
    yield runner
    runner.stop()

@pytest.fixture
def garage_sqlite_full(tmpdir: Path):
    runner = GarageRunner(tmpdir, "sqlite_full")
    yield runner
    runner.stop()