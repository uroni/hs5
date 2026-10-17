from pathlib import Path

import pytest
import os
import shutil
import tempfile

def pytest_collection_modifyitems(config: pytest.Config, items: list[pytest.Item]):
    if os.getenv("CI"):
        return

    skip_slow = pytest.mark.skip(reason="Slow test, only runs in CI (set CI=1 to run)")
    for item in items:
        if "slow" in item.keywords:
            item.add_marker(skip_slow)

@pytest.fixture(scope="session", autouse=True)
def init_port(testrun_uid: str):
    port_path = Path(f"/tmp/hs5-ports-{testrun_uid}")
    if port_path.exists():
        print("Port files already exist, skipping initialization.")
        return

    # Populate a private directory first and rename it into place atomically,
    # so other xdist workers never see a partially written port directory
    tmp_path = Path(tempfile.mkdtemp(prefix=f"hs5-ports-{testrun_uid}-", dir="/tmp"))
    with open(tmp_path / "hs5", "w") as pf:
        pf.write("11000")

    with open(tmp_path / "minio", "w") as pf:
        pf.write("12000")

    with open(tmp_path / "garage", "w") as pf:
        pf.write("14000")

    with open(tmp_path / "rustfs", "w") as pf:
        pf.write("13000")

    try:
        os.rename(tmp_path, port_path)
    except OSError:
        shutil.rmtree(tmp_path)
        print("Port files already exist, skipping initialization.")

