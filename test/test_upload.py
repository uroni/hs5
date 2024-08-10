# Copyright Martin Raiber. All Rights Reserved.
# SPDX-License-Identifier: LGPL-3.0-or-later
from concurrent.futures import thread
from dataclasses import dataclass
import datetime
from distutils.command.upload import upload
import json
import logging
from pathlib import Path
import random
from re import T
from typing import AnyStr, Optional, Union
from uuid import uuid4
import boto3
from botocore.exceptions import ClientError
import os
from hs5_fixture import Hs5Runner, hs5, hs5_large
import pytest
import threading
import binascii
from boto3.s3.transfer import TransferConfig
import time
import filecmp
import io
import requests

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

    fdata = os.urandom(29*1024*1024)
    with open(tmp_path / "upload.txt", "wb") as upload_file:
        upload_file.write(fdata)
    
    s3_client = hs5.get_s3_client()
    with io.FileIO(tmp_path / "upload.txt", "rb") as upload_file:
        s3_client.put_object(Bucket="testbucket", Key="upload.txt", Body=upload_file)

    obj_info = s3_client.head_object(Bucket="testbucket", Key="upload.txt")
    assert obj_info["ContentLength"] == len(fdata)

    obj_range = s3_client.get_object(Bucket="testbucket", Key="upload.txt", Range="bytes=0-9")
    bdata = obj_range["Body"].read()
    assert len(bdata) == 10
    assert bdata == fdata[:10]

    obj_range = s3_client.get_object(Bucket="testbucket", Key="upload.txt", Range="bytes=10-19")
    assert obj_range["Body"].read() == fdata[10:20]

    dl_path = tmp_path / "download.txt"
    s3_client.download_file("testbucket", "upload.txt", str(dl_path))
    assert os.stat(dl_path).st_size == len(fdata)

    with open(dl_path, "rb") as f:
        rdata = f.read()
        assert len(rdata) == len(fdata)
        assert rdata == fdata

    hs5.commit_storage(s3_client)

    list_resp = s3_client.list_objects(Bucket="testbucket")

    assert not list_resp["IsTruncated"]
    objs = list_resp["Contents"]
    assert len(objs) == 1
    lm = objs[0]["LastModified"]
    now = datetime.datetime.now(tz=lm.tzinfo)
    diff = now - lm
    assert diff.total_seconds() < 60
    assert "Key" in objs[0] and objs[0]["Key"] == "upload.txt"
    assert "Size" in objs[0] and objs[0]["Size"] == len(fdata)
    
    s3_client.delete_object(Bucket="testbucket", Key="upload.txt")
    with pytest.raises(ClientError):
        s3_client.delete_object(Bucket="testbucket", Key="upload_nonexistent.txt")
    with pytest.raises(ClientError):
        s3_client.download_file("testbucket", "upload.txt", str(dl_path))

    hs5.commit_storage(s3_client)

    list_resp = s3_client.list_objects(Bucket="testbucket")
    assert not list_resp["IsTruncated"]
    assert "Contents" not in list_resp

def test_get_commit_obj(tmp_path: Path, hs5: Hs5Runner):
    s3_client = hs5.get_s3_client()
    fpath = tmp_path / "commit_uuid.txt"
    s3_client.download_file("testbucket", "a711e93e-93b4-4a9e-8a0b-688797470002", str(fpath))

    with open(fpath, "r") as f:
        assert len(f.read())>30

def add_objects(tmp_path: Path, hs5: Hs5Runner) -> set[str]:
    with open(tmp_path / "upload.txt", "w") as upload_file:
        upload_file.write("abc")

    s3_client = hs5.get_s3_client()
    ul_files = set[str]()
    for i in range(0, 210):
        s3name = f"{i}.txt"
        s3_client.upload_file(upload_file.name, "testbucket", s3name)
        ul_files.add(s3name)

    hs5.commit_storage(s3_client)

    return ul_files

def test_multipage_list(tmp_path: Path, hs5: Hs5Runner):

    ul_files = add_objects(tmp_path, hs5)

    s3_client = hs5.get_s3_client()

    marker : Optional[str]= None
    while True:
        if marker:
            res = s3_client.list_objects(Bucket="testbucket", Marker=marker, MaxKeys=100)
        else:
            res = s3_client.list_objects(Bucket="testbucket", MaxKeys=100)

        objs = res["Contents"]
        assert len(objs)<=100
        for obj in objs:
            assert "Key" in obj
            assert obj["Key"] in ul_files
            assert "Size" in obj and obj["Size"] == 3
            ul_files.remove(obj["Key"])
            s3_client.delete_object(Bucket="testbucket", Key=obj["Key"])
            marker = obj["Key"]

        if not res["IsTruncated"]:
            break

    assert not ul_files
    
    hs5.commit_storage(s3_client)

    res = s3_client.list_objects(Bucket="testbucket", MaxKeys=100)
    assert "Contents" not in res

def test_multipage_list_v2(tmp_path: Path, hs5: Hs5Runner):

    ul_files = add_objects(tmp_path, hs5)

    s3_client = hs5.get_s3_client()

    continuation_token : Optional[str]= None
    while True:
        if continuation_token:
            res = s3_client.list_objects_v2(Bucket="testbucket", ContinuationToken=continuation_token, MaxKeys=100)
        else:
            res = s3_client.list_objects_v2(Bucket="testbucket", MaxKeys=100)

        objs = res["Contents"]
        assert len(objs)<=100
        for obj in objs:
            assert "Key" in obj
            assert obj["Key"] in ul_files
            assert "Size" in obj and obj["Size"] == 3
            ul_files.remove(obj["Key"])
            s3_client.delete_object(Bucket="testbucket", Key=obj["Key"])

        if not res["IsTruncated"]:
            break

        continuation_token = res["NextContinuationToken"]

    assert not ul_files
    
    hs5.commit_storage(s3_client)

    res = s3_client.list_objects(Bucket="testbucket", MaxKeys=100)
    assert "Contents" not in res


def test_list_prefix(tmp_path: Path, hs5: Hs5Runner):
    with open(tmp_path / "upload.txt", "w") as upload_file:
        upload_file.write("abc")

    s3_client = hs5.get_s3_client()

    s3_client.upload_file(upload_file.name, "testbucket", "1.txt")
    s3_client.upload_file(upload_file.name, "testbucket", "a/2.txt")
    s3_client.upload_file(upload_file.name, "testbucket", "b/3.txt")

    hs5.commit_storage(s3_client)

    res = s3_client.list_objects(Bucket="testbucket", Prefix="a/")

    objs = res["Contents"]
    assert objs is not None
    assert len(objs) == 1
    assert objs[0]["Key"] == "a/2.txt"


def test_list_delim(tmp_path: Path, hs5: Hs5Runner):
    with open(tmp_path / "upload.txt", "w") as upload_file:
        upload_file.write("abc")

    s3_client = hs5.get_s3_client()

    s3_client.upload_file(upload_file.name, "testbucket", "1.txt")
    s3_client.upload_file(upload_file.name, "testbucket", "a/2.txt")
    s3_client.upload_file(upload_file.name, "testbucket", "b/3.txt")

    hs5.commit_storage(s3_client)

    res = s3_client.list_objects(Bucket="testbucket", Delimiter="/")

    objs = res["Contents"]
    assert objs is not None
    assert len(objs) == 1
    assert objs[0]["Key"] == "1.txt"

    common_prefixes = res["CommonPrefixes"]
    prefixes = [pre["Prefix"] for pre in common_prefixes]

    assert len(prefixes) == 2
    assert "a/" in prefixes
    assert "b/" in prefixes


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


def test_put_large(hs5_large: Hs5Runner, tmp_path: Path):
    tmpfile = tmp_path / "ulfile.dat"
    with open(tmpfile, "wb") as f:
        fsize = 0
        while fsize<1*1024*1024*1024:
            f.write(os.urandom(512*1024))
            fsize += 512*1024

    s3_client = hs5_large.get_s3_client()

    with open(tmpfile, "rb") as f:
        s3_client.put_object(Bucket="testbucket", Key="upload.dat", Body=f)

    dl_path = tmp_path / "download.dat"
    s3_client.download_file("testbucket", "upload.dat", str(dl_path))

    assert filecmp.cmp(tmpfile, dl_path)

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


def test_login(hs5: Hs5Runner):
    url = hs5.get_api_url()

    ses = requests.session()

    response = ses.post(url+"login", json={
        "username": "root",
        "password": hs5.get_root_key()
    })

    assert response.status_code == 200
    resp = response.json()
    assert resp["ses"]


def test_adduser(hs5: Hs5Runner):
    url = hs5.get_api_url()

    ses1 = requests.session()

    response = ses1.post(url+"adduser", json={
        "ses": "foobar",
        "username": "test",
        "password": "test"
    })

    assert response.status_code == 401

    response = ses1.post(url+"login", json={
        "username": "root",
        "password": hs5.get_root_key()
    })

    assert response.status_code == 200
    resp = response.json()
    sesVal = resp["ses"]

    response = ses1.post(url+"adduser", json={
        "ses": sesVal,
        "username": "test",
        "password": "test"
    })

    assert response.status_code == 200

    ses2 = requests.session()

    response = ses2.post(url+"login", json={
        "username": "test",
        "password": "test"
    })

    assert response.status_code == 200

