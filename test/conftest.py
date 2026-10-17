from pathlib import Path

import pytest
import os

def pytest_collection_modifyitems(config: pytest.Config, items: list[pytest.Item]):
    if os.getenv("CI"):
        return

    skip_ci_only = pytest.mark.skip(reason="Only runs in CI (set CI=1 to run)")
    for item in items:
        if "ci_only" in item.keywords:
            item.add_marker(skip_ci_only)

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

