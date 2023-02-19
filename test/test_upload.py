# Copyright Martin Raiber. All Rights Reserved.
# SPDX-License-Identifier: AGPL-3.0-or-later

from concurrent.futures import thread
from dataclasses import dataclass
from distutils.command.upload import upload
import logging
from pathlib import Path
from re import T
from uuid import uuid4
import boto3
from botocore.exceptions import ClientError
import os
from hs5_fixture import Hs5Runner, hs5
import pytest
import threading
import binascii
from boto3.s3.transfer import TransferConfig
import time
import filecmp

def create_random_file(fn: Path, size: int) -> int:
    with open(fn, "wb") as f:
        csize = 0
        crc = 0
        while csize < size:
            towrite = min(size-csize, 512*1024)
            data = os.urandom(towrite)
            #data = bytearray(towrite)
            f.write(data)
            crc = binascii.crc32(data, crc)
            csize += towrite

    return crc



def test_put_get_del_list(tmp_path: Path, hs5: Hs5Runner):

    with open(tmp_path / "upload.txt", "w") as upload_file:
        upload_file.write("abc")
    
    s3_client = hs5.get_s3_client()
    s3_client.upload_file(upload_file.name, "testbucket", "upload.txt")
    dl_path = tmp_path / "download.txt"
    s3_client.download_file("testbucket", "upload.txt", str(dl_path))

    with open(dl_path, "r") as f:
        assert f.read() == "abc"

    list_resp = s3_client.list_objects(Bucket="testbucket")

    assert not list_resp["IsTruncated"]
    objs = list_resp["Contents"]
    assert len(objs) == 1
    assert "Key" in objs[0] and objs[0]["Key"] == "testbucket/upload.txt"
    assert "Size" in objs[0] and objs[0]["Size"] == 3
    
    s3_client.delete_object(Bucket="testbucket", Key="upload.txt")
    with pytest.raises(ClientError):
        s3_client.download_file("testbucket", "upload.txt", str(dl_path))

    list_resp = s3_client.list_objects(Bucket="testbucket")
    assert not list_resp["IsTruncated"]
    assert "Contents" not in list_resp

def test_get_commit_obj(tmp_path: Path, hs5: Hs5Runner):
    s3_client = hs5.get_s3_client()
    fpath = tmp_path / "commit_uuid.txt"
    s3_client.download_file("testbucket", "a711e93e-93b4-4a9e-8a0b-688797470002", str(fpath))

    with open(fpath, "r") as f:
        assert len(f.read())>30

def test_put_multipart(tmp_path: Path, hs5: Hs5Runner):

    with open(tmp_path / "upload_multipart.dat", "wb") as upload_file:
        size = 50*1024*1024

        while size > 0:
            buf = os.urandom(512*1024)
            upload_file.write(buf)
            size -= len(buf)

    s3_client = hs5.get_s3_client()
    s3_client.upload_file(upload_file.name, "testbucket", "upload.txt")
    dl_path = tmp_path / "download.dat"
    s3_client.download_file("testbucket", "upload.txt", str(dl_path))

    assert filecmp.cmp(upload_file.name, dl_path)


def test_put_get_del_stress(tmp_path: Path, hs5: Hs5Runner):
    s3_client = hs5.get_s3_client()

    @dataclass
    class DlInfo:
        allow_throttle = True
        running_downloads = 0

    def put_get_del(n: int, obj_size: int, throttle: bool, dl_info: DlInfo):
        for i in range(0, n):
            fname = uuid4().hex + ".dat"
            fpath = tmp_path / fname
            ul_crc = create_random_file(fpath, obj_size)

            config = TransferConfig(multipart_threshold=5*1024*1024*1024)

            s3_client.upload_file(str(fpath), "testbucket", fname, Config=config)

            def del_thread(fpath):
                s3_client.delete_object(Bucket="testbucket", Key=fname)

            t = threading.Thread(target=del_thread, args=(fpath,))

            resp = s3_client.get_object(Bucket="testbucket", Key=fname)
            crc = 0
            body = resp["Body"]
            cl = resp["ContentLength"]
            assert cl == obj_size
            cl -= 1
            b1 = body.read(1)
            crc = binascii.crc32(b1, crc)
            
            dl_info.running_downloads += 1

            t.start()
            
            if throttle:
                while cl > 0:
                    b2 = body.read(min(4096, cl))
                    crc = binascii.crc32(b2, crc)
                    if dl_info.allow_throttle:
                        time.sleep(0.01)
                    cl-=len(b2)
            else:
                b2 = body.read()
                crc = binascii.crc32(b2, crc)

            assert crc == ul_crc

            t.join()

            dl_info.running_downloads -= 1

    dl_info = DlInfo()

    t0 = threading.Thread(target=put_get_del, args=(1, 90*1024*1024, True, dl_info))
    t0.start()

    while dl_info.running_downloads==0:
        time.sleep(0.1)

    threads : list[threading.Thread] = []
    for i in range(0, 1):
        t = threading.Thread(target=put_get_del, args=(100, 1*1024*1024, False, dl_info))
        t.start()
        threads.append(t)

    for t in threads:
        t.join()

    dl_info.allow_throttle = False

    t0.join()



