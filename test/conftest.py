from pathlib import Path

import pytest
import os

@pytest.fixture(scope="session", autouse=True)
def init_port(testrun_uid: str):
    port_path = Path(f"/tmp/hs5-ports-{testrun_uid}")
    try:
        port_path.mkdir(exist_ok=False)
        with open(port_path / "hs5", "w") as pf:
            pf.write("11000")

        with open(port_path / "minio", "w") as pf:
            pf.write("12000")

        with open(port_path / "garage", "w") as pf:
            pf.write("14000")

        with open(port_path / "rustfs", "w") as pf:
            pf.write("13000")
    except FileExistsError:
        print("Port files already exist, skipping initialization.")

