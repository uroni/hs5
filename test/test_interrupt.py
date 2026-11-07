from dataclasses import dataclass
import hashlib
import http.client
import io
import os
from pathlib import Path
import random
import socket
import struct
import time
from urllib.parse import urlsplit
from uuid import uuid4

from mypy_boto3_s3 import S3Client
import pytest
from hs5_fixture import Hs5Runner, hs5, hs5_get_prepare_delay


@dataclass
class UploadResult:
    name: str
    md5: str


def upload_file(hs5: Hs5Runner, tmp_path: Path, s3_client: S3Client) -> UploadResult:

    if random.choice([True, False]):
        fdata = os.urandom(1 * 1024 * 1024 + 52)
    else:
        fdata = os.urandom(3000)

    tpath = tmp_path / (uuid4().hex + ".dat")
    with open(tpath, "wb") as upload_file:
        upload_file.write(fdata)

    fdata_md5 = hashlib.md5(fdata).hexdigest()

    with io.FileIO(tpath, "rb") as upload_file:
        res = s3_client.put_object(
            Bucket=hs5.testbucketname(), Key=tpath.name, Body=upload_file
        )

    return UploadResult(name=tpath.name, md5=fdata_md5)


def verify_files(
    hs5: Hs5Runner, uploaded_files: dict[str, UploadResult], s3_client: S3Client
):

    for upload_result in uploaded_files.values():
        obj = s3_client.get_object(Bucket=hs5.testbucketname(), Key=upload_result.name)
        body = obj["Body"].read()
        body_md5 = hashlib.md5(body).hexdigest()
        assert (
            body_md5 == upload_result.md5
        ), f"MD5 mismatch for file {upload_result.name}"

    objs = s3_client.list_objects_v2(Bucket=hs5.testbucketname())
    keys_in_bucket = set[str]()
    if "Contents" in objs:
        for item in objs["Contents"]:
            assert "Key" in item
            keys_in_bucket.add(item["Key"])

    assert keys_in_bucket == set(
        uploaded_files.keys()
    ), "Bucket contents do not match uploaded files"


def test_put_then_get(hs5: Hs5Runner, tmp_path: Path):

    s3_client = hs5.get_s3_client()

    uploaded_files = dict[str, UploadResult]()

    result = upload_file(hs5, tmp_path, s3_client)
    uploaded_files[result.name] = result

    verify_files(hs5, uploaded_files, s3_client)


def test_random_interrupt(tmp_path: Path, hs5: Hs5Runner):

    uploaded_files = dict[str, UploadResult]()
    s3_client = hs5.get_s3_client()

    for i in range(100):

        rnd_action = random.choice(["upload", "upload", "delete", "reset"])

        if rnd_action == "reset":
            hs5.restart()
        if rnd_action == "upload":
            result = upload_file(hs5, tmp_path, s3_client)
            uploaded_files[result.name] = result
        else:
            if uploaded_files:
                file_to_delete = random.choice(list(uploaded_files.values()))

                s3_client.delete_object(
                    Bucket=hs5.testbucketname(), Key=file_to_delete.name
                )
                del uploaded_files[file_to_delete.name]

    verify_files(hs5, uploaded_files, s3_client)


@pytest.mark.slow
def test_disconnect_during_put(hs5: Hs5Runner):
    """Client disconnects during PutObject uploads while the body thread is still writing"""
    s3_client = hs5.get_s3_client()
    # Below the data file size limit of the hs5 fixture
    total_len = 96 * 1024 * 1024
    chunk = os.urandom(1024 * 1024)

    for i in range(10):
        url = urlsplit(
            s3_client.generate_presigned_url(
                "put_object",
                Params={"Bucket": hs5.testbucketname(), "Key": f"disconnect{i}"},
                ExpiresIn=600,
            )
        )
        send_len = (8 + (i % 8) * 8) * 1024 * 1024

        conn = http.client.HTTPConnection(url.hostname, url.port, timeout=30)
        conn.putrequest("PUT", f"{url.path}?{url.query}", skip_accept_encoding=True)
        conn.putheader("Content-Length", str(total_len))
        conn.endheaders()
        for _ in range(send_len // len(chunk)):
            conn.send(chunk)
        conn.sock.shutdown(socket.SHUT_RDWR)
        conn.close()

    s3_client.put_object(
        Bucket=hs5.testbucketname(), Key="after_disconnect", Body=b"ok"
    )
    obj = s3_client.get_object(Bucket=hs5.testbucketname(), Key="after_disconnect")
    assert obj["Body"].read() == b"ok"


def test_reset_during_get(hs5_get_prepare_delay: Hs5Runner):
    """Client resets the connection while a GET is prepared, so the response is ready after onError"""
    hs5 = hs5_get_prepare_delay
    s3_client = hs5.get_s3_client()
    s3_client.put_object(Bucket=hs5.testbucketname(), Key="obj", Body=b"data")

    for _ in range(3):
        url = urlsplit(
            s3_client.generate_presigned_url(
                "get_object",
                Params={"Bucket": hs5.testbucketname(), "Key": "obj"},
                ExpiresIn=600,
            )
        )
        sock = socket.create_connection((url.hostname, url.port))
        sock.sendall(
            f"GET {url.path}?{url.query} HTTP/1.1\r\nHost: {url.netloc}\r\n\r\n".encode()
        )
        time.sleep(0.2)
        # Close with RST instead of FIN
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        sock.close()
        # Wait for the delayed GET preparation to finish
        time.sleep(1.5)

    obj = s3_client.get_object(Bucket=hs5.testbucketname(), Key="obj")
    assert obj["Body"].read() == b"data"
