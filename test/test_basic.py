# Copyright Martin Raiber. All Rights Reserved.
# SPDX-License-Identifier: LGPL-3.0-or-later
from concurrent.futures import thread
from dataclasses import dataclass
import datetime
import json
import logging
from pathlib import Path
import random
from typing import AnyStr, Optional, Union
from unittest.mock import patch
from uuid import uuid4
import boto3
from botocore.exceptions import ClientError
import os
from hs5_fixture import Hs5Runner, hs5, hs5_large, hs5_large_small_alloc_chunksize
import pytest
import threading
import binascii
from boto3.s3.transfer import TransferConfig
import time
import filecmp
import io
import requests
import hashlib
from hs5_commit import Hs5Commit, Hs5RestartError
from awsgosdktest import UploadAwsSdkGo, goawssdk_test_fixture
from miniosdktest import UploadMinioGo, miniosdk_test_fixture

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

def test_head_bucket(hs5: Hs5Runner):
    s3_client = hs5.get_s3_client()

    hb = s3_client.head_bucket(Bucket=hs5.testbucketname())
    assert hb["ResponseMetadata"]["HTTPStatusCode"] == 200

    with pytest.raises(ClientError):
        s3_client.head_bucket(Bucket=hs5.testbucketname() + "-nonexistent")


def test_put_get_del_list(tmp_path: Path, hs5: Hs5Runner):

    assert hs5.get_stats().used == 0

    fdata = os.urandom(29*1024*1024)
    with open(tmp_path / "upload.txt", "wb") as upload_file:
        upload_file.write(fdata)

    fdata_md5 = hashlib.md5(fdata).hexdigest()

    
    s3_client = hs5.get_s3_client()
    with io.FileIO(tmp_path / "upload.txt", "rb") as upload_file:
        res = s3_client.put_object(Bucket=hs5.testbucketname(), Key="upload.txt", Body=upload_file, ContentType="text/markdown")
        assert res["ETag"].strip('"').lower() == fdata_md5.lower()

    obj_info = s3_client.head_object(Bucket=hs5.testbucketname(), Key="upload.txt")
    assert obj_info["ContentLength"] == len(fdata)
    assert obj_info["LastModified"].year >= 2025
    assert obj_info["ContentType"] == "text/markdown"

    obj_range = s3_client.get_object(Bucket=hs5.testbucketname(), Key="upload.txt", Range="bytes=0-9")
    assert obj_range["ContentType"] == "text/markdown"
    assert obj_range["LastModified"].year >= 2025
    bdata = obj_range["Body"].read()
    assert len(bdata) == 10
    assert bdata == fdata[:10]

    obj_range = s3_client.get_object(Bucket=hs5.testbucketname(), Key="upload.txt", Range="bytes=-10")
    bdata = obj_range["Body"].read()
    assert len(bdata) == 10
    assert bdata == fdata[-10:]

    if os.getenv("ENABLE_WAL_WRITE_DATA") == "1":
        time.sleep(1)
        
    assert hs5.get_stats().used == len(fdata)

    obj_range = s3_client.get_object(Bucket=hs5.testbucketname(), Key="upload.txt", Range="bytes=10-19")
    assert obj_range["ContentType"] == "text/markdown"
    assert obj_range["Body"].read() == fdata[10:20]

    with pytest.raises(ClientError, match="Range Not Satisfiable"):
        obj_range = s3_client.get_object(Bucket=hs5.testbucketname(), Key="upload.txt", Range=f"bytes={100*1024*1024}-{100*1024*1024 +9}")

    dl_path = tmp_path / "download.txt"
    s3_client.download_file(hs5.testbucketname(), "upload.txt", str(dl_path))
    assert os.stat(dl_path).st_size == len(fdata)

    with open(dl_path, "rb") as f:
        rdata = f.read()
        assert len(rdata) == len(fdata)
        assert rdata == fdata

    hs5.commit_storage(s3_client)

    list_resp = s3_client.list_objects(Bucket=hs5.testbucketname())

    assert not list_resp["IsTruncated"]
    objs = list_resp["Contents"]
    assert len(objs) == 1
    assert "LastModified" in objs[0]
    lm = objs[0]["LastModified"]
    now = datetime.datetime.now(tz=lm.tzinfo)
    diff = now - lm
    assert diff.total_seconds() < 60
    assert "Key" in objs[0] and objs[0]["Key"] == "upload.txt"
    assert "Size" in objs[0] and objs[0]["Size"] == len(fdata)
    assert "ETag" in objs[0] and objs[0]["ETag"].strip('"').lower() == fdata_md5.lower()
    
    s3_client.delete_object(Bucket=hs5.testbucketname(), Key="upload.txt")
    s3_client.delete_object(Bucket=hs5.testbucketname(), Key="upload_nonexistent.txt")
    with pytest.raises(ClientError):
        s3_client.download_file(hs5.testbucketname(), "upload.txt", str(dl_path))

    hs5.commit_storage(s3_client)

    list_resp = s3_client.list_objects(Bucket=hs5.testbucketname())
    assert not list_resp["IsTruncated"]
    assert "Contents" not in list_resp

    assert hs5.get_stats().used == 0

def test_put_get_commit_context(tmp_path: Path, hs5: Hs5Runner):
    s3_client = hs5.get_s3_client()

    fdata = os.urandom(1*1024*1024)
    with open(tmp_path / "upload.txt", "wb") as upload_file:
        upload_file.write(fdata)

    with Hs5Commit(s3_client, hs5.testbucketname()) as commit:
        with io.FileIO(tmp_path / "upload.txt", "rb") as upload_file:
            s3_client.put_object(Bucket=hs5.testbucketname(), Key="upload.txt", Body=upload_file)

    hs5.restart()

    dl_path = tmp_path / "download.txt"
    s3_client.download_file(hs5.testbucketname(), "upload.txt", str(dl_path))
    assert os.stat(dl_path).st_size == len(fdata)

def test_put_get_commit_context_restart_fail(tmp_path: Path, hs5: Hs5Runner):
    if not hs5.manual_commit:
        pytest.skip("Skip test in manual commit disabled mode")

    s3_client = hs5.get_s3_client()

    fdata = os.urandom(1*1024*1024)
    with open(tmp_path / "upload.txt", "wb") as upload_file:
        upload_file.write(fdata)

    with pytest.raises(Hs5RestartError):
        with Hs5Commit(s3_client, hs5.testbucketname()) as commit:
            with io.FileIO(tmp_path / "upload.txt", "rb") as upload_file:
                s3_client.put_object(Bucket=hs5.testbucketname(), Key="upload.txt", Body=upload_file)

            hs5.restart()

    with pytest.raises(ClientError):
        dl_path = tmp_path / "download.txt"
        s3_client.download_file(hs5.testbucketname(), "upload.txt", str(dl_path))

    

def test_get_commit_obj(tmp_path: Path, hs5: Hs5Runner):
    s3_client = hs5.get_s3_client()
    fpath = tmp_path / "commit_uuid.txt"
    s3_client.download_file(hs5.testbucketname(), "a711e93e-93b4-4a9e-8a0b-688797470002", str(fpath))

    with open(fpath, "r") as f:
        assert len(f.read())>5

def add_objects(tmp_path: Path, hs5: Hs5Runner, num_objects : int = 210, object_data = b"abc", bucketname = None) -> set[str]:
    if bucketname is None:
        bucketname = hs5.testbucketname()

    with open(tmp_path / "upload.txt", "wb") as upload_file:
        upload_file.write(object_data)

    s3_client = hs5.get_s3_client()
    ul_files = set[str]()
    for i in range(0, num_objects):
        s3name = f"{i}.txt"
        s3_client.upload_file(upload_file.name, bucketname, s3name)
        ul_files.add(s3name)

    hs5.commit_storage(s3_client)

    return ul_files

def delete_all_objects(hs5: Hs5Runner, bucket: str):
    s3_client = hs5.get_s3_client()
    res = s3_client.list_objects(Bucket=bucket)
    assert not res["IsTruncated"]
    # TODO: Use multi-page listing
    if "Contents" in res:
        for obj in res["Contents"]:
            assert "Key" in obj
            s3_client.delete_object(Bucket=bucket, Key=obj["Key"])

    hs5.commit_storage(s3_client)

    assert hs5.get_stats().used == 0


def test_put_empty(tmp_path: Path, hs5: Hs5Runner):
    fdata = ""
    with open(tmp_path / "upload.txt", "w") as upload_file:
        upload_file.write(fdata)
    
    s3_client = hs5.get_s3_client()
    with io.FileIO(tmp_path / "upload.txt", "rb") as upload_file:
        s3_client.put_object(Bucket=hs5.testbucketname(), Key="upload.txt", Body=upload_file)

    obj_info = s3_client.head_object(Bucket=hs5.testbucketname(), Key="upload.txt")
    assert obj_info["ContentLength"] == len(fdata)
    assert obj_info["ContentType"] == "binary/octet-stream"

    dl_path = tmp_path / "download.txt"
    s3_client.download_file(hs5.testbucketname(), "upload.txt", str(dl_path))
    assert os.stat(dl_path).st_size == len(fdata)


def test_list_buckets(hs5: Hs5Runner):
    s3_client = hs5.get_s3_client()
    buckets = s3_client.list_buckets()
    assert "Buckets" in buckets
    bucket_names = [b["Name"] for b in buckets["Buckets"]]
    assert hs5.testbucketname() in bucket_names

def test_multipage_list(tmp_path: Path, hs5: Hs5Runner):

    ul_files = add_objects(tmp_path, hs5)

    s3_client = hs5.get_s3_client()

    marker : Optional[str]= None
    while True:
        if marker:
            res = s3_client.list_objects(Bucket=hs5.testbucketname(), Marker=marker, MaxKeys=100)
        else:
            res = s3_client.list_objects(Bucket=hs5.testbucketname(), MaxKeys=100)

        objs = res["Contents"]
        assert len(objs)<=100
        for obj in objs:
            assert "Key" in obj
            assert obj["Key"] in ul_files
            assert "Size" in obj and obj["Size"] == 3
            ul_files.remove(obj["Key"])
            s3_client.delete_object(Bucket=hs5.testbucketname(), Key=obj["Key"])
            marker = obj["Key"]

        if not res["IsTruncated"]:
            break

    assert not ul_files
    
    hs5.commit_storage(s3_client)

    res = s3_client.list_objects(Bucket=hs5.testbucketname(), MaxKeys=100)
    assert "Contents" not in res

def test_multipage_list_v2(tmp_path: Path, hs5: Hs5Runner):

    ul_files = add_objects(tmp_path, hs5)

    s3_client = hs5.get_s3_client()

    continuation_token : Optional[str]= None
    while True:
        if continuation_token:
            res = s3_client.list_objects_v2(Bucket=hs5.testbucketname(), ContinuationToken=continuation_token, MaxKeys=100)
        else:
            res = s3_client.list_objects_v2(Bucket=hs5.testbucketname(), MaxKeys=100)

        objs = res["Contents"]
        assert len(objs)<=100
        for obj in objs:
            assert "Key" in obj
            assert obj["Key"] in ul_files
            assert "Size" in obj and obj["Size"] == 3
            ul_files.remove(obj["Key"])
            s3_client.delete_object(Bucket=hs5.testbucketname(), Key=obj["Key"])

        if not res["IsTruncated"]:
            break

        continuation_token = res["NextContinuationToken"]

    assert not ul_files
    
    hs5.commit_storage(s3_client)

    res = s3_client.list_objects(Bucket=hs5.testbucketname(), MaxKeys=100)
    assert "Contents" not in res

def test_list_v2_empty(hs5: Hs5Runner):
    s3_client = hs5.get_s3_client()

    res = s3_client.list_objects_v2(Bucket=hs5.testbucketname(), MaxKeys=100)
    assert not res["IsTruncated"]
    assert "Contents" not in res

    # Test with a prefix
    res = s3_client.list_objects_v2(Bucket=hs5.testbucketname(), Prefix="nonexistent", MaxKeys=100)
    assert not res["IsTruncated"]
    assert "Contents" not in res


def test_list_prefix(tmp_path: Path, hs5: Hs5Runner):
    with open(tmp_path / "upload.txt", "w") as upload_file:
        upload_file.write("abc")

    s3_client = hs5.get_s3_client()

    s3_client.upload_file(upload_file.name, hs5.testbucketname(), "1.txt")
    s3_client.upload_file(upload_file.name, hs5.testbucketname(), "a/2.txt")
    s3_client.upload_file(upload_file.name, hs5.testbucketname(), "b/3.txt")

    hs5.commit_storage(s3_client)

    res = s3_client.list_objects(Bucket=hs5.testbucketname(), Prefix="a/")

    objs = res["Contents"]
    assert objs is not None
    assert len(objs) == 1
    assert "Key" in objs[0]
    assert objs[0]["Key"] == "a/2.txt"


def test_list_delim(tmp_path: Path, hs5: Hs5Runner):
    with open(tmp_path / "upload.txt", "w") as upload_file:
        upload_file.write("abc")

    s3_client = hs5.get_s3_client()

    s3_client.upload_file(upload_file.name, hs5.testbucketname(), "1.txt")
    s3_client.upload_file(upload_file.name, hs5.testbucketname(), "a/2.txt")
    s3_client.upload_file(upload_file.name, hs5.testbucketname(), "b/3.txt")

    hs5.commit_storage(s3_client)

    res = s3_client.list_objects(Bucket=hs5.testbucketname(), Delimiter="/")

    objs = res["Contents"]
    assert objs is not None
    assert len(objs) == 1
    assert "Key" in objs[0]
    assert objs[0]["Key"] == "1.txt"

    common_prefixes = res["CommonPrefixes"]
    prefixes = list[str]()
    for pre in common_prefixes:
        assert "Prefix" in pre
        prefixes.append(pre["Prefix"])

    assert len(prefixes) == 2
    assert "a/" in prefixes
    assert "b/" in prefixes

def test_list_one(tmp_path: Path, hs5: Hs5Runner):
    with open(tmp_path / "upload.txt", "w") as upload_file:
        upload_file.write("abc")

    s3_client = hs5.get_s3_client()

    s3_client.upload_file(upload_file.name, hs5.testbucketname(), "one")
    s3_client.upload_file(upload_file.name, hs5.testbucketname(), "two")
    s3_client.upload_file(upload_file.name, hs5.testbucketname(), "three")

    hs5.commit_storage(s3_client)

    keys = list[str]()

    cont = True
    contToken : str | None = None
    while cont:
        args = {}
        if contToken:
            args["ContinuationToken"] = contToken
        res = s3_client.list_objects_v2(Bucket=hs5.testbucketname(), MaxKeys=1, **args)

        assert "Contents" in res
        objs = res["Contents"]
        assert objs is not None
        assert len(objs) == 1
        assert "Key" in objs[0]
        keys.append(objs[0]["Key"])
        cont = res["IsTruncated"]
        contToken = res.get("NextContinuationToken")

    assert keys == ["one", "three", "two"]

def test_list_start_after(tmp_path: Path, hs5: Hs5Runner):
    with open(tmp_path / "upload.txt", "w") as upload_file:
        upload_file.write("abc")

    s3_client = hs5.get_s3_client()

    for fn in ["a", "b", "c", "d"]:
        s3_client.upload_file(upload_file.name, hs5.testbucketname(), fn)

    objs = s3_client.list_objects_v2(Bucket=hs5.testbucketname(), StartAfter="b")
    assert "Contents" in objs
    assert len(objs["Contents"]) == 2

    assert "Key" in objs["Contents"][0]
    assert objs["Contents"][0]["Key"] == "c"
    


def test_put_multipart(tmp_path: Path, hs5: Hs5Runner):

    assert hs5.get_stats().used == 0

    ul_size = 50*1024*1024

    with open(tmp_path / "upload_multipart.dat", "wb") as upload_file:
        size = ul_size
        while size > 0:
            buf = os.urandom(512*1024)
            upload_file.write(buf)
            size -= len(buf)

    s3_client = hs5.get_s3_client()

    MB = 1024 * 1024
    config = TransferConfig(multipart_chunksize=8 * MB)

    s3_client.upload_file(upload_file.name, hs5.testbucketname(), "upload.txt", Config=config, ExtraArgs={'ContentType': 'text/plain'})

    hs5.commit_storage(s3_client)

    assert hs5.get_stats().used == ul_size

    dl_path = tmp_path / "download.dat"   
    s3_client.download_file(hs5.testbucketname(), "upload.txt", str(dl_path))

    assert filecmp.cmp(upload_file.name, dl_path)

    obj_range = s3_client.get_object(Bucket=hs5.testbucketname(), Key="upload.txt", Range="bytes=0-9")
    assert obj_range["ContentType"] == "text/plain"
    bdata = obj_range["Body"].read()
    assert len(bdata) == 10

    obj_range = s3_client.get_object(Bucket=hs5.testbucketname(), Key="upload.txt", Range="bytes=10-19")
    bdata = obj_range["Body"].read()
    assert len(bdata) == 10

    off = 20*1024*1024
    obj_range = s3_client.get_object(Bucket=hs5.testbucketname(), Key="upload.txt", Range=f"bytes={off}-{off+9}")
    bdata = obj_range["Body"].read()
    assert len(bdata) == 10

    off = 20*1024*1024
    size = off
    obj_range = s3_client.get_object(Bucket=hs5.testbucketname(), Key="upload.txt", Range=f"bytes={off}-{off+size -1}")
    bdata = obj_range["Body"].read()
    assert len(bdata) == size

    obj_range = s3_client.get_object(Bucket=hs5.testbucketname(), Key="upload.txt", PartNumber=1)
    bdata = obj_range["Body"].read()
    md5sum = hashlib.md5(bdata).hexdigest()
    assert len(bdata) == 8*MB
    with open(upload_file.name, "rb") as f:
        assert bdata == f.read(8*MB)
    assert obj_range["ETag"].strip('"').lower() == md5sum.lower()

    obj_range = s3_client.get_object(Bucket=hs5.testbucketname(), Key="upload.txt", PartNumber=2)
    bdata = obj_range["Body"].read()
    md5sum = hashlib.md5(bdata).hexdigest()
    assert len(bdata) == 8*MB
    with open(upload_file.name, "rb") as f:
        f.seek(8*MB)
        assert bdata == f.read(8*MB) 
    assert obj_range["ETag"].strip('"').lower() == md5sum.lower()

    obj = s3_client.head_object(Bucket=hs5.testbucketname(), Key="upload.txt")
    assert obj["ContentLength"] == ul_size

    obj = s3_client.head_object(Bucket=hs5.testbucketname(), Key="upload.txt", PartNumber=2)
    assert obj["ContentLength"] == 8*MB
    assert obj["ETag"].strip('"').lower() == md5sum.lower()

    with pytest.raises(ClientError):
        s3_client.head_object(Bucket=hs5.testbucketname(), Key="upload.txt", PartNumber=(70*MB)//(8*MB))

    with pytest.raises(ClientError): 
        s3_client.head_object(Bucket=hs5.testbucketname(), Key="upload.txt", PartNumber=0)

    with pytest.raises(ClientError):
        s3_client.get_object(Bucket=hs5.testbucketname(), Key="upload.txt", PartNumber=100)

    objs = s3_client.list_objects(Bucket=hs5.testbucketname())
    assert "Contents" in objs
    assert len(objs["Contents"]) == 1
    assert "Key" in objs["Contents"][0]
    assert objs["Contents"][0]["Key"] == "upload.txt"
    assert "Size" in objs["Contents"][0]
    assert objs["Contents"][0]["Size"] == ul_size


    s3_client.delete_object(Bucket=hs5.testbucketname(), Key="upload.txt")

    hs5.commit_storage(s3_client)

    assert hs5.get_stats().used == 0


def test_list_partial_uploads(tmp_path: Path, hs5: Hs5Runner):

    assert hs5.get_stats().used == 0

    s3_client = hs5.get_s3_client()

    key = "partial_upload.dat"
    part_path = tmp_path / "multipart_part.bin"
    with open(part_path, "wb") as part_file:
        part_file.write(os.urandom(6*1024*1024))

    create_resp = s3_client.create_multipart_upload(Bucket=hs5.testbucketname(), Key=key)
    upload_id = create_resp["UploadId"]

    with open(part_path, "rb") as part_file:
        s3_client.upload_part(
            Bucket=hs5.testbucketname(),
            Key=key,
            PartNumber=1,
            UploadId=upload_id,
            Body=part_file
        )

    hs5.commit_storage(s3_client)

    list_resp = s3_client.list_multipart_uploads(Bucket=hs5.testbucketname())
    assert not list_resp["IsTruncated"]
    assert "Uploads" in list_resp
    uploads = list_resp["Uploads"]
    assert len(uploads) == 1
    assert "Key" in uploads[0] and uploads[0]["Key"] == key
    assert "UploadId" in uploads[0] and uploads[0]["UploadId"] == upload_id

    s3_client.abort_multipart_upload(Bucket=hs5.testbucketname(), Key=key, UploadId=upload_id)

    hs5.commit_storage(s3_client)

    list_resp = s3_client.list_multipart_uploads(Bucket=hs5.testbucketname())
    assert not list_resp["IsTruncated"]
    assert "Uploads" not in list_resp

    assert hs5.get_stats().used == 0


def test_put_large(hs5_large: Hs5Runner, tmp_path: Path):

    tmpfile = tmp_path / "ulfile.dat"
    with open(tmpfile, "wb") as f:
        fsize = 0
        while fsize<1*1024*1024:
            f.write(os.urandom(512*1024))
            fsize += 512*1024

    s3_client = hs5_large.get_s3_client()
    with open(tmpfile, "rb") as f:
        s3_client.put_object(Bucket=hs5_large.testbucketname(), Key="upload.dat", Body=f)

    dl_path = tmp_path / "download.dat"
    s3_client.download_file(hs5_large.testbucketname(), "upload.dat", str(dl_path))

    assert filecmp.cmp(tmpfile, dl_path)

@pytest.mark.parametrize("size", [512*1024, 5*1024*1024+12, 101, 51*1024*1024])
@pytest.mark.parametrize("checksum_algorithm", ["CRC32", "CRC32C", "SHA1", "SHA256", "CRC64NVME"])
def test_put_chunked(hs5_large: Hs5Runner, tmp_path: Path, size: int, checksum_algorithm: str):

    tmpfile = tmp_path / f"ulfile_{size}.dat"
    with open(tmpfile, "wb") as f: 
        f.write(os.urandom(size))

    import botocore.httpchecksum
    def resolve_override(request, operation_model, params):
        botocore.httpchecksum.resolve_request_checksum_algorithm(request, operation_model, params)
        botocore.httpchecksum.resolve_response_checksum_algorithms(request, operation_model, params)
        if operation_model.name == "PutObject":
            checksum_context = request["context"]["checksum"]
            checksum_context["request_algorithm"]["in"] = "trailer"

    with patch("botocore.httpchecksum.DEFAULT_CHECKSUM_ALGORITHM", new=checksum_algorithm):
        with patch("botocore.client.resolve_checksum_context", new=resolve_override):
            s3_client = hs5_large.get_s3_client()

            tmpfile = tmp_path / f"ulfile_{size}.dat"
            with open(tmpfile, "rb") as f:
                s3_client.put_object(Bucket=hs5_large.testbucketname(), Key=f"upload_{size}.dat", Body=f)

            dl_path = tmp_path / f"download_{size}.dat"
            s3_client.download_file(hs5_large.testbucketname(), f"upload_{size}.dat", str(dl_path))

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

            s3_client.upload_file(str(fpath), hs5.testbucketname(), fname, Config=config)

            def del_thread(fpath):
                s3_client.delete_object(Bucket=hs5.testbucketname(), Key=fname)

            t = threading.Thread(target=del_thread, args=(fpath,))

            resp = s3_client.get_object(Bucket=hs5.testbucketname(), Key=fname)
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

@pytest.mark.parametrize("sig_v2", [True, False])
def test_download_presigned(tmp_path: Path, hs5: Hs5Runner, sig_v2: bool):

    fdata = os.urandom(29*1024*1024)
    with open(tmp_path / "upload.txt", "wb") as upload_file:
        upload_file.write(fdata)
    
    s3_client = hs5.get_s3_client(sig_v2=sig_v2)
    with io.FileIO(tmp_path / "upload.txt", "rb") as upload_file:
        s3_client.put_object(Bucket=hs5.testbucketname(), Key="upload.txt", Body=upload_file)

    # get presigned donwload URL
    presigned_url = s3_client.generate_presigned_url(
        'get_object',
        Params={'Bucket': hs5.testbucketname(), 'Key': 'upload.txt'},
        ExpiresIn=3600
    )

    response = requests.get(presigned_url)
    response.raise_for_status()
    assert response.content == fdata


def test_create_bucket(hs5: Hs5Runner, tmp_path: Path):

    s3_client = hs5.get_s3_client()

    bucketname = "testbucket-created"

    s3_client.create_bucket(Bucket=bucketname)
    with pytest.raises(ClientError):
        s3_client.create_bucket(Bucket=bucketname)

    with pytest.raises(ClientError):
        s3_client.create_bucket(Bucket="INVALID")

    fdata = os.urandom(2*1024)
    with open(tmp_path / "upload.txt", "wb") as upload_file:
        upload_file.write(fdata)
    
    with io.FileIO(tmp_path / "upload.txt", "rb") as upload_file:
        s3_client.put_object(Bucket=bucketname, Key="upload.txt", Body=upload_file)

    obj_info = s3_client.head_object(Bucket=bucketname, Key="upload.txt")
    assert obj_info["ContentLength"] == len(fdata)

    with pytest.raises(ClientError):
        s3_client.delete_bucket(Bucket=bucketname)

    s3_client.delete_object(Bucket=bucketname, Key="upload.txt")

    s3_client.delete_bucket(Bucket=bucketname)


def test_upload_long_key(hs5: Hs5Runner, tmp_path: Path):
    s3_client = hs5.get_s3_client()

    key = "a" * 1024
    with open(tmp_path / "upload.txt", "w") as upload_file:
        upload_file.write("abc")

    s3_client.upload_file(upload_file.name, hs5.testbucketname(), key)

    with pytest.raises(ClientError):
        with io.FileIO(tmp_path / "upload.txt", "rb") as upload_file:
            s3_client.put_object(Bucket=hs5.testbucketname(), Key= "a" * 1025, Body=upload_file)

    dl_path = tmp_path / "download.txt"
    s3_client.download_file(hs5.testbucketname(), key, str(dl_path))
    assert os.stat(dl_path).st_size == 3
    
    list = s3_client.list_objects(Bucket=hs5.testbucketname())
    assert not list["IsTruncated"]
    assert "Contents" in list
    objs = list["Contents"]
    assert len(objs) == 1
    assert "Key" in objs[0] and objs[0]["Key"] == key

    s3_client.delete_object(Bucket=hs5.testbucketname(), Key=key)

    list = s3_client.list_objects(Bucket=hs5.testbucketname())
    assert not list["IsTruncated"]
    assert "Contents" not in list

@pytest.mark.skipif(os.environ.get("ENABLE_WAL") == "1" and os.environ.get("ENABLE_WAL_WRITE_META") != "0", reason="With metadata WAL, space reuse is delayed to WAL commit")
def test_datafile_space_reused(hs5_large_small_alloc_chunksize: Hs5Runner):
    """
    Make sure that the datafile space is reused after a delete
    """
    hs5 = hs5_large_small_alloc_chunksize

    def add_remove():
        mb_data = os.urandom(1024*1024)

        add_objects(hs5._workdir, hs5, 100, mb_data)
        delete_all_objects(hs5, hs5.testbucketname())

    for _ in range(0, 2):
        add_remove()

    assert hs5.get_datafile_size() < 120*1024*1024
    
def test_delete_multiple_objects(tmp_path: Path, hs5: Hs5Runner):
    s3_client = hs5.get_s3_client()
    
    # Create multiple objects
    for i in range(5):
        with open(tmp_path / f"upload_{i}.txt", "w") as upload_file:
            upload_file.write(f"Content of file {i}")
        s3_client.upload_file(str(tmp_path / f"upload_{i}.txt"), "testbucket", f"upload_{i}.txt")

    hs5.commit_storage(s3_client)

    # Delete objects
    objects_to_delete = [{'Key': f'upload_{i}.txt'} for i in range(5)]
    response = s3_client.delete_objects(
        Bucket='testbucket',
        Delete={
            'Objects': objects_to_delete
        }
    )

    # Check if all objects were deleted
    assert response['ResponseMetadata']['HTTPStatusCode'] == 200
    assert 'Deleted' in response and len(response['Deleted']) == 5

    # Verify deletion
    for i in range(5):
        with pytest.raises(ClientError):
            s3_client.head_object(Bucket='testbucket', Key=f'upload_{i}.txt')


    objects_to_delete = [{'Key': f'nonexistent_{i}.txt'} for i in range(5)]
    response = s3_client.delete_objects(
        Bucket='testbucket',
        Delete={
            'Objects': objects_to_delete
        }
    )
    # Check if all objects were reported as deleted (even if they didn't exist)
    assert response['ResponseMetadata']['HTTPStatusCode'] == 200
    assert 'Deleted' in response and len(response['Deleted']) == 5


def test_url_encoded(hs5: Hs5Runner, tmp_path: Path):
    
    s3_client = hs5.get_s3_client()

    tmpf = tmp_path / "file with spaces(1).txt"

    with open(tmpf, "w") as upload_file:
        upload_file.write("abc")

    s3_client.upload_file(str(tmpf), hs5.testbucketname(), tmpf.name)

    # Verify HEAD works with URL-encoded key
    obj_info = s3_client.head_object(Bucket=hs5.testbucketname(), Key=tmpf.name)
    assert obj_info["ContentLength"] == 3

    # List bucket and verify object name
    list_resp = s3_client.list_objects(Bucket=hs5.testbucketname())
    assert not list_resp["IsTruncated"]
    assert "Contents" in list_resp
    objs = list_resp["Contents"]
    assert len(objs) == 1
    assert "Key" in objs[0] and objs[0]["Key"] == tmpf.name

    dl_path = tmp_path / "download.txt"
    s3_client.download_file(hs5.testbucketname(), tmpf.name, dl_path)

    with open(dl_path, "r") as f:
        data = f.read()
        assert data == "abc"

    # Delete the object and verify it's deleted via listing
    s3_client.delete_object(Bucket=hs5.testbucketname(), Key=tmpf.name)
    
    list_resp = s3_client.list_objects(Bucket=hs5.testbucketname())
    assert not list_resp["IsTruncated"]
    assert "Contents" not in list_resp

def test_read_commit_info(hs5: Hs5Runner, tmp_path: Path):
    if not hs5.manual_commit:
        pytest.skip("Skipping read commit info test in automatic commit mode")

    s3_client = hs5.get_s3_client()

    runtime_id_io = io.BytesIO()
    s3_client.download_fileobj(hs5.testbucketname(), "a711e93e-93b4-4a9e-8a0b-688797470002", runtime_id_io)
    runtime_id_io.seek(0)
    rid = runtime_id_io.read().decode()

    assert len(rid) > 5

    obj = s3_client.get_object(Bucket=hs5.testbucketname(), Key="a711e93e-93b4-4a9e-8a0b-688797470002")
    data = obj["Body"].read().decode()
    assert data == rid


@pytest.mark.asyncio
@pytest.mark.skipif(os.environ.get("AIOBOTO3_TEST") != "1", reason="Requires aioboto3")
async def test_read_commit_info_async(hs5: Hs5Runner):
    if not hs5.manual_commit:
        pytest.skip("Skipping read commit info test in automatic commit mode")

    import aioboto3
    s3_client = aioboto3.Session(aws_access_key_id="root", aws_secret_access_key=hs5._root_key)
    async with s3_client.client("s3", endpoint_url=hs5.get_url()) as s3:
        runtime_id_io = io.BytesIO()
        await s3.download_fileobj(hs5.testbucketname(), "a711e93e-93b4-4a9e-8a0b-688797470002", runtime_id_io)
        runtime_id_io.seek(0)
        rid = runtime_id_io.read().decode()

        assert len(rid) > 5

@pytest.mark.skipif(os.environ.get("GOLANG_TEST") != "1", reason="Requires golang")
@pytest.mark.parametrize("size", [512*1024, 5*1024*1024+12, 101, 51*1024*1024]) 
def test_awsgosdk_upload(hs5: Hs5Runner, tmp_path: Path, goawssdk_test_fixture: UploadAwsSdkGo, size: int):
    s3_client = hs5.get_s3_client()

    fdata = os.urandom(size)
    with open(tmp_path / "upload.txt", "wb") as upload_file:
        upload_file.write(fdata)

    goawssdk_test_fixture.upload(hs5.testbucketname(), "upload.txt", tmp_path / "upload.txt")

    obj_info = s3_client.head_object(Bucket=hs5.testbucketname(), Key="upload.txt")
    assert obj_info["ContentLength"] == len(fdata)

    dl_path = tmp_path / "download.txt"
    s3_client.download_file(hs5.testbucketname(), "upload.txt", str(dl_path))
    assert filecmp.cmp(tmp_path / "upload.txt", dl_path)

    dl_path2 = tmp_path / "download2.txt"
    goawssdk_test_fixture.download(hs5.testbucketname(), "upload.txt",
        dl_path2)
    assert filecmp.cmp(tmp_path / "upload.txt", dl_path2)   

@pytest.mark.skipif(os.environ.get("GOLANG_TEST") != "1", reason="Requires golang")
@pytest.mark.parametrize("size", [512*1024, 5*1024*1024+12, 101, 51*1024*1024])                                                      
def test_miniosdk_upload(hs5: Hs5Runner, tmp_path: Path, miniosdk_test_fixture: UploadMinioGo, size: int):
    s3_client = hs5.get_s3_client()

    fdata = os.urandom(size)
    with open(tmp_path / "upload.txt", "wb") as upload_file:
        upload_file.write(fdata)

    miniosdk_test_fixture.upload(hs5.testbucketname(), "upload.txt", tmp_path / "upload.txt")

    obj_info = s3_client.head_object(Bucket=hs5.testbucketname(), Key="upload.txt")
    assert obj_info["ContentLength"] == len(fdata)

    dl_path = tmp_path / "download.txt"
    s3_client.download_file(hs5.testbucketname(), "upload.txt", str(dl_path))
    assert filecmp.cmp(tmp_path / "upload.txt", dl_path)

    dl_path2 = tmp_path / "download2.txt"
    miniosdk_test_fixture.download(hs5.testbucketname(), "upload.txt", dl_path2)
    assert filecmp.cmp(tmp_path / "upload.txt", dl_path2)


