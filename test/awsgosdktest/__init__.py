from pathlib import Path

import pytest
from sh import go # type: ignore
import sh
import sys

from hs5_fixture import Hs5Runner

class UploadAwsSdkGo:
    def __init__(self, hs5: Hs5Runner) -> None:
        self._hs5 = hs5

    def action(self, bucket: str, key: str, fn: Path, action: str) -> None:
        gosdktest = sh.Command("test/awsgosdktest/gosdktest")
        gosdktest(self._hs5.get_url(), "root", self._hs5.get_root_key(), fn, bucket, key, action, _fg=True)
    
    def upload(self, bucket: str, key: str, fn: Path) -> None:
        self.action(bucket, key, fn, "upload")

    def download(self, bucket: str, key: str, fn: Path) -> None:
        self.action(bucket, key, fn, "download")

    def listbuckets(self, bucket: str) -> None:
        self.action(bucket, "", Path(""), "listbuckets")

@pytest.fixture
def goawssdk_test_fixture(hs5: Hs5Runner):
    if not Path("test/awsgosdktest/gosdktest").exists():
        try:
            go("mod", "init", "test/awsgosdktest", _fg=True)
        except: # type: ignore
            # Module already initialized, ignore
            pass
        go("mod", "tidy", _cwd="test/awsgosdktest",  _fg=True)
        go("build", "-o", "gosdktest", "main.go", _cwd="test/awsgosdktest", _fg=True)
    yield UploadAwsSdkGo(hs5)

