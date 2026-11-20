# Copyright Martin Raiber. All Rights Reserved.
# SPDX-License-Identifier: LGPL-3.0-or-later
import hashlib
import os
import random
import socket
import sqlite3
from pathlib import Path
import threading
import time
import uuid

import pytest
import redis
from mypy_boto3_s3.type_defs import ObjectVersionTypeDef

from hs5_fixture import Hs5Runner, hs5_redis, hs5_redis_large


def version_id(version: ObjectVersionTypeDef) -> str:
    vid = version.get("VersionId")
    assert vid is not None
    return vid


def test_redis_strings(hs5_redis: Hs5Runner):
    r = hs5_redis.get_redis_client()
    assert r.ping()

    assert r.get("a") is None
    assert r.set("a", b"1")
    assert r.get("a") == b"1"
    assert r.strlen("a") == 1

    assert r.set("a", b"x", nx=True) is None
    assert r.get("a") == b"1"
    assert r.set("b", b"x", xx=True) is None
    assert r.get("b") is None
    assert r.set("a", b"2", xx=True)
    assert r.set("a", b"3", get=True) == b"2"
    assert r.get("a") == b"3"
    assert r.setnx("a", b"4") is False
    assert r.setnx("c", b"4") is True

    assert r.mset({"d": b"d1", "e": b"e1"})
    assert r.mget(["d", "e", "missing"]) == [b"d1", b"e1", None]

    assert r.incr("counter") == 1
    assert r.incrby("counter", 10) == 11
    assert r.decr("counter") == 10
    assert r.decrby("counter", 20) == -10
    assert r.get("counter") == b"-10"
    r.set("notint", b"abc")
    with pytest.raises(redis.ResponseError, match="not an integer"):
        r.incr("notint")

    assert r.exists("a", "a", "missing") == 2
    assert r.delete("a", "missing") == 1
    assert r.get("a") is None
    assert r.exists("a") == 0

    assert r.set("x", b"1", ex=10)
    assert 9 <= r.ttl("x") <= 10

    big = bytes(range(256)) * 10000
    assert r.set("big", big)
    assert r.get("big") == big


def test_redis_s3_strings(hs5_redis: Hs5Runner):
    r = hs5_redis.get_redis_client()
    s3 = hs5_redis.get_s3_client()
    bucket = hs5_redis.redis_bucket()

    assert s3.get_bucket_versioning(Bucket=bucket)["Status"] == "Enabled"

    # Redis -> S3
    r.set("key1", b"value1")
    r.set("key1", b"value2")
    assert s3.get_object(Bucket=bucket, Key="key1")["Body"].read() == b"value2"
    # SET replaces the old versions
    versions = s3.list_object_versions(Bucket=bucket, Prefix="key1")
    assert len(versions["Versions"]) == 1

    # S3 -> Redis
    s3.put_object(Bucket=bucket, Key="key2", Body=b"from s3")
    assert r.get("key2") == b"from s3"

    # Delete marker hides the value
    s3.delete_object(Bucket=bucket, Key="key2")
    assert r.get("key2") is None
    assert r.exists("key2") == 0

    assert r.set("key2", b"again")
    assert s3.get_object(Bucket=bucket, Key="key2")["Body"].read() == b"again"
    versions = s3.list_object_versions(Bucket=bucket, Prefix="key2")
    assert len(versions.get("Versions", [])) == 1
    assert len(versions.get("DeleteMarkers", [])) == 0


def test_redis_lists(hs5_redis: Hs5Runner):
    r = hs5_redis.get_redis_client()

    assert r.lpush("l", b"b", b"a") == 2
    assert r.rpush("l", b"c", b"d") == 4
    assert r.lpush("l", b"0") == 5
    assert r.llen("l") == 5
    assert r.lrange("l", 0, -1) == [b"0", b"a", b"b", b"c", b"d"]
    assert r.lrange("l", 1, 2) == [b"a", b"b"]
    assert r.lrange("l", -2, -1) == [b"c", b"d"]
    assert r.lindex("l", 0) == b"0"
    assert r.lindex("l", -1) == b"d"
    assert r.lindex("l", 10) is None

    assert r.lpop("l") == b"0"
    assert r.rpop("l") == b"d"
    assert r.lpop("l", 2) == [b"a", b"b"]
    assert r.rpop("l", 5) == [b"c"]
    assert r.lpop("l") is None
    assert r.rpop("l", 2) is None
    assert r.llen("l") == 0
    assert r.exists("l") == 0

    assert r.lpushx("l", b"x") == 0
    assert r.rpush("l", b"x") == 1
    assert r.rpushx("l", "y") == 2
    assert r.lrange("l", 0, -1) == [b"x", b"y"]
    assert r.type("l") == b"list"
    assert r.delete("l") == 1
    assert r.type("l") == b"none"


def test_redis_s3_lists(hs5_redis: Hs5Runner):
    r = hs5_redis.get_redis_client()
    s3 = hs5_redis.get_s3_client()
    bucket = hs5_redis.redis_bucket()

    r.rpush("q", b"1", b"2", b"3")
    r.lpush("q", b"0")

    # Head of the list is the current object
    assert s3.get_object(Bucket=bucket, Key="q")["Body"].read() == b"0"

    versions = s3.list_object_versions(Bucket=bucket, Prefix="q")["Versions"]
    assert len(versions) == 4

    # Version ids of prepended (negative) versions work via S3
    bodies = [
        s3.get_object(Bucket=bucket, Key="q", VersionId=version_id(v))["Body"].read()
        for v in versions
    ]
    assert bodies == [b"0", b"1", b"2", b"3"]

    # Delete an element via S3
    s3.delete_object(Bucket=bucket, Key="q", VersionId=version_id(versions[2]))
    assert r.lrange("q", 0, -1) == [b"0", b"1", b"3"]

    # S3 PUT with list element content type adds to the head of the list
    s3.put_object(
        Bucket=bucket, Key="q", Body=b"s3", ContentType="application/cdmi-queue"
    )
    assert r.lpop("q") == b"s3"

    # Listing with a version marker of a prepended element
    page = s3.list_object_versions(
        Bucket=bucket,
        Prefix="q",
        KeyMarker="q",
        VersionIdMarker=version_id(versions[1]),
    )
    assert [
        s3.get_object(Bucket=bucket, Key="q", VersionId=version_id(v))["Body"].read()
        for v in page["Versions"]
    ] == [b"3"]

    assert r.rpop("q") == b"3"
    assert r.lrange("q", 0, -1) == [b"0", b"1"]


def test_redis_blocking_pop(hs5_redis: Hs5Runner):
    # redis-py returns a list with RESP3, a tuple with RESP2
    def bpop(v):
        return tuple(v) if v is not None else None

    r = hs5_redis.get_redis_client()

    start = time.monotonic()
    assert r.brpop(["bq"], timeout=0.5) is None
    assert time.monotonic() - start >= 0.4

    r.rpush("bq2", b"x")
    assert bpop(r.blpop(["bq", "bq2"], timeout=1)) == (b"bq2", b"x")

    def push_later():
        time.sleep(0.5)
        hs5_redis.get_redis_client().lpush("bq", b"pushed")

    t = threading.Thread(target=push_later)
    t.start()
    start = time.monotonic()
    assert bpop(r.brpop(["bq"], timeout=10)) == (b"bq", b"pushed")
    # Woken up by the push, not by polling
    assert time.monotonic() - start < 0.9
    t.join()

    # Objects added via S3 are found by polling
    s3 = hs5_redis.get_s3_client()

    def put_later():
        time.sleep(0.5)
        s3.put_object(
            Bucket=hs5_redis.redis_bucket(),
            Key="bq",
            Body=b"s3",
            ContentType="application/cdmi-queue",
        )

    t = threading.Thread(target=put_later)
    t.start()
    assert bpop(r.brpop(["bq"], timeout=10)) == (b"bq", b"s3")
    t.join()


def test_redis_concurrent_pops(hs5_redis: Hs5Runner):
    r = hs5_redis.get_redis_client()
    num = 200
    r.rpush("cq", *[str(i).encode() for i in range(num)])

    popped: list[bytes] = []
    lock = threading.Lock()

    def worker(left: bool):
        c = hs5_redis.get_redis_client()
        while True:
            v = c.lpop("cq") if left else c.rpop("cq")
            if v is None:
                return
            assert isinstance(v, bytes)
            with lock:
                popped.append(v)

    threads = [threading.Thread(target=worker, args=(i % 2 == 0,)) for i in range(8)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()

    assert sorted(popped, key=int) == [str(i).encode() for i in range(num)]


def test_redis_scan(hs5_redis: Hs5Runner):
    r = hs5_redis.get_redis_client()
    keys = {f"scan:{i:03d}".encode() for i in range(55)}
    for k in keys:
        r.set(k, b"v")
    r.set("other", b"v")

    assert set(r.scan_iter(match="scan:*", count=7)) == keys
    assert set(r.scan_iter(count=3)) == keys | {b"other"}
    assert set(r.keys("scan:00?")) == {f"scan:00{i}".encode() for i in range(10)}
    assert r.dbsize() == 56

    assert r.flushdb()
    assert r.dbsize() == 0
    assert r.keys("*") == []


def test_redis_auth(hs5_redis: Hs5Runner):
    r = hs5_redis.get_redis_client(authenticate=False)
    # NOAUTH reply
    with pytest.raises(redis.AuthenticationError, match="Authentication required"):
        r.get("a")

    with pytest.raises(redis.AuthenticationError):
        redis.Redis(
            host="127.0.0.1",
            port=hs5_redis.get_redis_port(),
            username="root",
            password="wrong",
        ).ping()


def test_redis_persistence(hs5_redis: Hs5Runner):
    r = hs5_redis.get_redis_client()
    r.set("p", b"persisted")
    r.rpush("pl", b"a", b"b")

    hs5_redis.commit_storage(hs5_redis.get_s3_client())
    hs5_redis.restart()

    r = hs5_redis.get_redis_client()
    assert r.get("p") == b"persisted"
    # Version counter continues after restart, so new elements are appended
    r.rpush("pl", b"c")
    r.lpush("pl", b"0")
    assert r.lrange("pl", 0, -1) == [b"0", b"a", b"b", b"c"]


def test_redis_pipeline(hs5_redis: Hs5Runner):
    r = hs5_redis.get_redis_client()
    pipe = r.pipeline(transaction=False)
    for i in range(100):
        pipe.rpush("pipe", str(i).encode())
    pipe.lrange("pipe", 0, -1)
    res = pipe.execute()
    assert res[:100] == list(range(1, 101))
    assert res[100] == [str(i).encode() for i in range(100)]


def test_redis_resp2(hs5_redis: Hs5Runner):
    r = hs5_redis.get_redis_client(protocol=2)
    assert r.get("missing") is None
    assert r.mget(["missing"]) == [None]
    assert r.lpop("missing", 2) is None
    assert r.brpop(["missing"], timeout=0.1) is None
    r.rpush("l", b"a")
    assert r.lrange("l", 0, -1) == [b"a"]
    assert r.blpop(["l"], timeout=1) == (b"l", b"a")


@pytest.mark.slow
def test_redis_large_values(hs5_redis_large: Hs5Runner):
    r = hs5_redis_large.get_redis_client()
    s3 = hs5_redis_large.get_s3_client()
    bucket = hs5_redis_large.redis_bucket()

    # Large value written via Redis, read via Redis and S3
    large = os.urandom(300 * 1024 * 1024)
    assert r.set("large", large)
    assert r.strlen("large") == len(large)
    assert r.get("large") == large
    obj = s3.get_object(Bucket=bucket, Key="large")
    assert obj["ContentLength"] == len(large)
    assert obj["ETag"] == f'"{hashlib.md5(large).hexdigest()}"'
    assert obj["Body"].read() == large
    del obj

    # Overwrite frees the old version
    large2 = os.urandom(200 * 1024 * 1024)
    assert r.set("large", large2)
    assert r.get("large") == large2
    assert len(s3.list_object_versions(Bucket=bucket, Prefix="large")["Versions"]) == 1
    del large, large2
    assert r.delete("large") == 1

    # Large list elements
    elems = [os.urandom(64 * 1024 * 1024 + i) for i in range(3)]
    assert r.rpush("largelist", elems[1], elems[2]) == 2
    assert r.lpush("largelist", elems[0]) == 3
    assert r.lindex("largelist", 1) == elems[1]
    assert r.lpop("largelist") == elems[0]
    assert r.rpop("largelist") == elems[2]
    assert r.lrange("largelist", 0, -1) == [elems[1]]
    del elems

    # Large multipart object uploaded via S3, read via Redis
    parts = [os.urandom(40 * 1024 * 1024) for _ in range(3)]
    upl = s3.create_multipart_upload(
        Bucket=bucket, Key="multipart", ContentType="application/cdmi-queue"
    )
    etags = [
        s3.upload_part(
            Bucket=bucket,
            Key="multipart",
            UploadId=upl["UploadId"],
            PartNumber=i + 1,
            Body=part,
        )["ETag"]
        for i, part in enumerate(parts)
    ]
    s3.complete_multipart_upload(
        Bucket=bucket,
        Key="multipart",
        UploadId=upl["UploadId"],
        MultipartUpload={
            "Parts": [{"ETag": e, "PartNumber": i + 1} for i, e in enumerate(etags)]
        },
    )
    # List element (content type)
    with pytest.raises(redis.ResponseError, match="WRONGTYPE"):
        r.get("multipart")
    assert r.lindex("multipart", 0) == b"".join(parts)
    # Pop the multipart object (deletes its parts)
    assert r.lpop("multipart") == b"".join(parts)
    assert r.exists("multipart") == 0


def test_redis_value_size_limit(tmpdir: Path, testrun_uid: str):
    loc = Path(tmpdir) / uuid.uuid4().hex
    loc.mkdir()
    runner = Hs5Runner(
        loc,
        data_file_size_limit_mb=100,
        testrun_uid=testrun_uid,
        with_redis=True,
        extra_args=["--redis_max_value_size", str(1024 * 1024)],
    )
    try:
        r = runner.get_redis_client()
        s3 = runner.get_s3_client()

        assert r.set("ok", b"x" * 1024 * 1024)
        assert r.get("ok") == b"x" * 1024 * 1024

        # Too large to write (protocol error, connection is closed)
        with pytest.raises(redis.RedisError):
            r.set("toolarge", b"x" * (1024 * 1024 + 1))
        assert r.get("toolarge") is None

        # Larger objects written via S3 cannot be read via Redis, only their size
        s3.put_object(
            Bucket=runner.redis_bucket(), Key="s3large", Body=b"y" * (2 * 1024 * 1024)
        )
        with pytest.raises(redis.ResponseError, match="too large"):
            r.get("s3large")
        assert r.strlen("s3large") == 2 * 1024 * 1024
    finally:
        runner.stop(cleanup=True, kill=False)


class RawRedisConnection:
    """Minimal RESP connection streaming values without buffering them"""

    def __init__(self, port: int):
        self.sock = socket.create_connection(("127.0.0.1", port))
        self.buf = b""

    def readline(self) -> bytes:
        while b"\r\n" not in self.buf:
            data = self.sock.recv(65536)
            assert data, "connection closed"
            self.buf += data
        line, self.buf = self.buf.split(b"\r\n", 1)
        return line

    def read_into(self, size: int, consume) -> None:
        """Reads size bytes, passing them to consume in chunks"""
        while size > 0:
            if not self.buf:
                self.buf = self.sock.recv(min(size, 4 * 1024 * 1024))
                assert self.buf, "connection closed"
            chunk = self.buf[:size]
            self.buf = self.buf[len(chunk) :]
            consume(chunk)
            size -= len(chunk)

    def command(self, *args: bytes) -> bytes:
        self.sock.sendall(
            b"*%d\r\n" % len(args)
            + b"".join(b"$%d\r\n%s\r\n" % (len(a), a) for a in args)
        )
        return self.readline()


@pytest.mark.slow
def test_redis_streaming_memory(tmpdir: Path, testrun_uid: str):
    """Large values are streamed to/from the data file instead of being buffered in memory"""
    size = 1024 * 1024 * 1024
    loc = Path(tmpdir) / uuid.uuid4().hex
    loc.mkdir()
    runner = Hs5Runner(
        loc,
        data_file_size_limit_mb=5000,
        testrun_uid=testrun_uid,
        with_redis=True,
        extra_args=["--redis_max_value_size", str(2 * size)],
    )
    try:
        conn = RawRedisConnection(runner.get_redis_port())
        assert conn.command(b"AUTH", b"root", runner.get_root_key().encode()) == b"+OK"

        baseline_mb = runner.get_rss_mb()
        chunk_size = 4 * 1024 * 1024
        rnd = random.Random(42)
        md5 = hashlib.md5()

        # Write
        with runner.track_rss() as rss:
            conn.sock.sendall(b"*3\r\n$3\r\nSET\r\n$4\r\nhuge\r\n$%d\r\n" % size)
            for _ in range(size // chunk_size):
                chunk = rnd.randbytes(chunk_size)
                md5.update(chunk)
                conn.sock.sendall(chunk)
            conn.sock.sendall(b"\r\n")
            assert conn.readline() == b"+OK"
        write_peak_mb = rss.peak_mb

        # Read back
        read_md5 = hashlib.md5()
        with runner.track_rss() as rss:
            conn.sock.sendall(b"*2\r\n$3\r\nGET\r\n$4\r\nhuge\r\n")
            assert conn.readline() == b"$%d" % size
            conn.read_into(size, read_md5.update)
            assert conn.readline() == b""
        read_peak_mb = rss.peak_mb

        assert read_md5.hexdigest() == md5.hexdigest()

        s3 = runner.get_s3_client()
        obj = s3.head_object(Bucket=runner.redis_bucket(), Key="huge")
        assert obj["ContentLength"] == size
        assert obj["ETag"] == f'"{md5.hexdigest()}"'

        # Memory use while streaming 1 GiB stays far below the value size
        assert (
            write_peak_mb - baseline_mb < 128
        ), f"RSS grew from {baseline_mb} MiB to {write_peak_mb} MiB while writing"
        assert (
            read_peak_mb - baseline_mb < 128
        ), f"RSS grew from {baseline_mb} MiB to {read_peak_mb} MiB while reading"

        # Popping streams as well
        assert conn.command(b"RPUSH", b"hugelist", b"small") == b":1"
        conn.sock.sendall(
            b"*3\r\n$5\r\nLPUSH\r\n$8\r\nhugelist\r\n$%d\r\n" % (100 * chunk_size)
        )
        rnd = random.Random(7)
        push_md5 = hashlib.md5()
        for _ in range(100):
            chunk = rnd.randbytes(chunk_size)
            push_md5.update(chunk)
            conn.sock.sendall(chunk)
        conn.sock.sendall(b"\r\n")
        assert conn.readline() == b":2"
        conn.sock.sendall(b"*2\r\n$4\r\nLPOP\r\n$8\r\nhugelist\r\n")
        assert conn.readline() == b"$%d" % (100 * chunk_size)
        pop_md5 = hashlib.md5()
        conn.read_into(100 * chunk_size, pop_md5.update)
        assert conn.readline() == b""
        assert pop_md5.hexdigest() == push_md5.hexdigest()
        assert conn.command(b"LLEN", b"hugelist") == b":1"
    finally:
        runner.stop(cleanup=True, kill=False)


def test_redis_pipeline_ordering(hs5_redis: Hs5Runner):
    """Pipelined commands are executed in batches, but replies stay in order"""
    r = hs5_redis.get_redis_client()
    port = hs5_redis.get_redis_port()

    # Large (streamed) reply in the middle of a pipeline
    large = os.urandom(3 * 1024 * 1024)
    r.set("large", large)
    pipe = r.pipeline(transaction=False)
    pipe.set("a", b"1")
    pipe.get("large")
    pipe.incr("a")
    pipe.get("large")
    pipe.get("a")
    assert pipe.execute() == [True, large, 2, large, b"2"]

    # Blocking command in the middle of a pipeline: following commands run after it is unblocked
    conn = RawRedisConnection(port)
    assert conn.command(b"AUTH", b"root", hs5_redis.get_root_key().encode()) == b"+OK"
    conn.sock.sendall(
        b"*3\r\n$5\r\nRPUSH\r\n$1\r\nq\r\n$1\r\nx\r\n"
        b"*3\r\n$5\r\nBLPOP\r\n$2\r\nbq\r\n$1\r\n0\r\n"
        b"*2\r\n$4\r\nLPOP\r\n$1\r\nq\r\n"
    )
    assert conn.readline() == b":1"
    time.sleep(0.5)
    # LPOP after the blocked BLPOP was not executed yet
    assert r.llen("q") == 1
    r.rpush("bq", b"woken")
    assert conn.readline() == b"*2"
    assert conn.readline() == b"$2"
    assert conn.readline() == b"bq"
    assert conn.readline() == b"$5"
    assert conn.readline() == b"woken"
    assert conn.readline() == b"$1"
    assert conn.readline() == b"x"

    # Commands after QUIT are not executed
    conn.sock.sendall(
        b"*3\r\n$3\r\nSET\r\n$1\r\nb\r\n$1\r\n1\r\n"
        b"*1\r\n$4\r\nQUIT\r\n"
        b"*3\r\n$3\r\nSET\r\n$1\r\nc\r\n$1\r\n1\r\n"
    )
    assert conn.readline() == b"+OK"
    assert conn.readline() == b"+OK"
    assert conn.sock.recv(1) == b""
    assert r.get("b") == b"1"
    assert r.get("c") is None


def test_redis_types(hs5_redis: Hs5Runner):
    r = hs5_redis.get_redis_client()
    s3 = hs5_redis.get_s3_client()
    bucket = hs5_redis.redis_bucket()

    r.set("s", b"v")
    r.rpush("l", b"a", b"b")
    assert r.type("s") == b"string"
    assert r.type("l") == b"list"
    assert r.type("missing") == b"none"

    # Types are stored as content type
    assert s3.head_object(Bucket=bucket, Key="s")["ContentType"] == "text/plain"
    assert (
        s3.head_object(Bucket=bucket, Key="l")["ContentType"]
        == "application/cdmi-queue"
    )

    # Wrong type operations
    for f in [
        lambda: r.get("l"),
        lambda: r.strlen("l"),
        lambda: r.incr("l"),
        lambda: r.set("l", b"x", get=True),
        lambda: r.lpush("s", b"x"),
        lambda: r.rpush("s", b"x"),
        lambda: r.lpushx("s", b"x"),
        lambda: r.lpop("s"),
        lambda: r.rpop("s", 2),
        lambda: r.llen("s"),
        lambda: r.lrange("s", 0, -1),
        lambda: r.lindex("s", 0),
        lambda: r.blpop(["s"], timeout=1),
    ]:
        with pytest.raises(redis.ResponseError, match="WRONGTYPE"):
            f()
    assert r.mget(["s", "l"]) == [b"v", None]
    assert r.get("s") == b"v"
    assert r.lrange("l", 0, -1) == [b"a", b"b"]

    # Objects uploaded via S3 are strings
    s3.put_object(Bucket=bucket, Key="s3obj", Body=b"x")
    assert r.type("s3obj") == b"string"
    with pytest.raises(redis.ResponseError, match="WRONGTYPE"):
        r.lpush("s3obj", b"y")

    # SET replaces lists
    assert r.set("l", b"now a string")
    assert r.type("l") == b"string"
    assert r.get("l") == b"now a string"
    versions = s3.list_object_versions(Bucket=bucket, Prefix="l")
    assert len(versions["Versions"]) == 1

    # SCAN TYPE
    assert set(r.scan_iter(_type="string")) == {b"s", b"s3obj", b"l"}
    r.lpush("l2", b"x")
    assert set(r.scan_iter(_type="list")) == {b"l2"}
    assert list(r.scan_iter(_type="hash")) == []


def test_redis_set_replaces_versions(hs5_redis: Hs5Runner):
    """SET writes the unversioned object and removes all other versions"""
    r = hs5_redis.get_redis_client()
    s3 = hs5_redis.get_s3_client()
    bucket = hs5_redis.redis_bucket()

    # Plain overwrite (fast path)
    for i in range(5):
        assert r.set("k", str(i).encode())
    assert r.get("k") == b"4"
    versions = s3.list_object_versions(Bucket=bucket, Prefix="k")
    assert len(versions["Versions"]) == 1
    assert "DeleteMarkers" not in versions

    # Newer S3 versions and delete markers are replaced
    s3.put_object(Bucket=bucket, Key="k", Body=b"s3 version")
    assert r.get("k") == b"s3 version"
    assert r.set("k", b"redis")
    assert r.get("k") == b"redis"
    assert s3.get_object(Bucket=bucket, Key="k")["Body"].read() == b"redis"
    s3.delete_object(Bucket=bucket, Key="k")
    assert r.get("k") is None
    assert r.set("k", b"again")
    assert r.get("k") == b"again"
    versions = s3.list_object_versions(Bucket=bucket, Prefix="k")
    assert len(versions["Versions"]) == 1
    assert "DeleteMarkers" not in versions

    # List with prepended and appended elements is replaced
    r.lpush("lst", b"a")
    r.rpush("lst", b"b")
    assert r.set("lst", b"str")
    assert r.get("lst") == b"str"
    assert len(s3.list_object_versions(Bucket=bucket, Prefix="lst")["Versions"]) == 1

    # INCR uses the same path
    assert r.incr("k2") == 1
    assert r.incr("k2") == 2
    assert len(s3.list_object_versions(Bucket=bucket, Prefix="k2")["Versions"]) == 1

    # Survives restart (WAL/commit)
    hs5_redis.commit_storage(hs5_redis.get_s3_client())
    hs5_redis.restart()
    r = hs5_redis.get_redis_client()
    assert r.get("k") == b"again"
    assert r.get("lst") == b"str"
    assert r.get("k2") == b"2"


def test_redis_list_length_counter(hs5_redis: Hs5Runner):
    """List lengths are counted by the storage engine, including modifications via S3"""
    r = hs5_redis.get_redis_client()
    s3 = hs5_redis.get_s3_client()
    bucket = hs5_redis.redis_bucket()

    assert r.llen("c") == 0
    assert r.lpush("c", b"a", b"b") == 2
    assert r.rpush("c", b"c") == 3
    assert r.llen("c") == 3
    assert r.lpop("c") == b"b"
    assert r.rpop("c", 2) == [b"c", b"a"]
    assert r.llen("c") == 0

    # Elements added/deleted via S3
    assert r.rpush("c", b"x", b"y") == 2
    s3.put_object(
        Bucket=bucket, Key="c", Body=b"s3", ContentType="application/cdmi-queue"
    )
    assert r.llen("c") == 3
    assert r.lpush("c", b"z") == 4
    versions = s3.list_object_versions(Bucket=bucket, Prefix="c")["Versions"]
    s3.delete_object(Bucket=bucket, Key="c", VersionId=version_id(versions[1]))
    assert r.llen("c") == 3
    assert r.lrange("c", 0, -1) == [b"z", b"x", b"y"]

    # Delete markers are no list elements
    s3.delete_object(Bucket=bucket, Key="c")
    assert r.llen("c") == 3
    assert r.rpush("c", b"w") == 4

    # Replacing/deleting the list
    assert r.set("c", b"str")
    with pytest.raises(redis.ResponseError, match="WRONGTYPE"):
        r.llen("c")
    assert r.delete("c") == 1
    assert r.llen("c") == 0
    assert r.rpush("c", b"1", b"2", b"3") == 3

    r.rpush("d", b"1")
    assert r.delete("d") == 1
    assert r.llen("d") == 0

    # Survives restart (WAL replay)
    hs5_redis.commit_storage(s3)
    hs5_redis.restart()
    r = hs5_redis.get_redis_client()
    assert r.llen("c") == 3
    assert r.lpush("c", b"0") == 4

    assert r.flushdb()
    assert r.llen("c") == 0
    assert r.rpush("c", b"new") == 1


def disable_redis_server(runner: Hs5Runner) -> list[str]:
    """Removes the Redis server arguments (for the next start). Returns them"""
    idx = runner.args.index("--redis_port")
    redis_args = runner.args[idx : idx + 2]
    del runner.args[idx : idx + 2]
    return redis_args


def set_redis_bucket_flag(runner: Hs5Runner, bucket: str, value: bool):
    """Sets the Redis bucket flag in the database (server has to be stopped)"""
    con = sqlite3.connect(Path(runner._workdir) / "hs5.db")
    con.execute("UPDATE buckets SET redis=? WHERE name=?", (1 if value else 0, bucket))
    con.commit()
    con.close()


def graceful_restart(runner: Hs5Runner):
    """Restart with a clean shutdown (commits everything, also with WAL or manual commit)"""
    runner.commit_storage(runner.get_s3_client())
    runner.stop(cleanup=False, kill=False)
    runner.start()
    runner._login_admin()


@pytest.mark.parametrize("via_api", [False, True])
def test_redis_bucket_delete_removes_counters(
    hs5_redis: Hs5Runner, capfd, via_api: bool
):
    if os.environ.get("ENABLE_WAL") == "1":
        # Counters of modifications still in the WAL are recomputed during WAL replay, so a stale counter cannot be
        # created by disabling counting for one run
        pytest.skip("Cannot create stale counter with WAL")

    r = hs5_redis.get_redis_client()
    s3 = hs5_redis.get_s3_client()
    bucket = hs5_redis.redis_bucket()

    r.rpush("l", b"a", b"b")
    versions = s3.list_object_versions(Bucket=bucket, Prefix="l")["Versions"]

    # Delete an element while the bucket is not marked as Redis bucket (and the Redis server is disabled, which
    # would mark it again), so the counter is stale
    redis_args = disable_redis_server(hs5_redis)
    hs5_redis.commit_storage(s3)
    hs5_redis.stop(cleanup=False, kill=False)
    set_redis_bucket_flag(hs5_redis, bucket, False)
    hs5_redis.start()
    hs5_redis._login_admin()
    s3.delete_object(Bucket=bucket, Key="l", VersionId=version_id(versions[0]))
    hs5_redis.args += redis_args
    graceful_restart(hs5_redis)

    r = hs5_redis.get_redis_client()
    assert r.llen("l") == 2
    s3.delete_object(Bucket=bucket, Key="l", VersionId=version_id(versions[1]))
    # Counter is not deleted (stale)
    assert r.llen("l") == 1

    capfd.readouterr()
    if via_api:
        response = hs5_redis._ses.post(
            hs5_redis.get_api_url() + "deleteBucket", json={"bucketName": bucket}
        )
        assert response.status_code == 200
    else:
        s3.delete_bucket(Bucket=bucket)
    out = capfd.readouterr()
    assert "Deleted 1 counters of bucket" in out.out + out.err


def test_redis_mset_atomic(hs5_redis: Hs5Runner):
    """MSET sets all keys atomically: a concurrent MGET never sees a mix of old and new values"""
    keys = [f"m{i}" for i in range(4)]
    r = hs5_redis.get_redis_client()
    r.mset({k: b"0" for k in keys})

    stop = threading.Event()
    mixed = []

    def reader():
        c = hs5_redis.get_redis_client()
        while not stop.is_set():
            values = c.mget(keys)
            if len(set(values)) != 1:
                mixed.append(values)
                return

    t = threading.Thread(target=reader)
    t.start()
    try:
        for i in range(1, 300):
            r.mset({k: str(i).encode() for k in keys})
            if mixed:
                break
    finally:
        stop.set()
        t.join()

    assert not mixed, f"MGET saw a partial MSET: {mixed[0]}"


def test_redis_counting_without_redis_server(hs5_redis: Hs5Runner):
    """Redis buckets are marked in the database, so list elements are counted also if the Redis server is disabled"""
    r = hs5_redis.get_redis_client()
    s3 = hs5_redis.get_s3_client()
    bucket = hs5_redis.redis_bucket()

    r.rpush("l", b"a", b"b", b"c")
    versions = s3.list_object_versions(Bucket=bucket, Prefix="l")["Versions"]

    redis_args = disable_redis_server(hs5_redis)
    graceful_restart(hs5_redis)
    s3.delete_object(Bucket=bucket, Key="l", VersionId=version_id(versions[0]))
    s3.put_object(
        Bucket=bucket, Key="l", Body=b"s3", ContentType="application/cdmi-queue"
    )
    hs5_redis.args += redis_args
    hs5_redis.commit_storage(s3)
    hs5_redis.restart()

    r = hs5_redis.get_redis_client()
    assert r.llen("l") == 3
    assert r.lrange("l", 0, -1) == [b"s3", b"b", b"c"]


@pytest.mark.slow
@pytest.mark.parametrize("value_size", [100, 20000])
def test_redis_inline_get_concurrent_overwrite(
    tmpdir: Path, testrun_uid: str, value_size: int
):
    """GETs read in the IO thread (get_in_worker_threshold) never see data of freed and reused extents while keys are
    overwritten concurrently
    """
    loc = Path(tmpdir) / uuid.uuid4().hex
    loc.mkdir()
    runner = Hs5Runner(
        loc,
        data_file_size_limit_mb=100,
        testrun_uid=testrun_uid,
        with_redis=True,
        # Small allocation chunks, so freed extents are reused quickly
        data_file_alloc_chunk_size=64 * 1024,
        extra_args=[
            "--get_in_worker_threshold=100000",
            # Widens the window between looking up and reading the value
            "--test_inline_read_delay_ms=100",
        ],
    )
    try:
        keys = [f"ow{i}" for i in range(4)]
        stop = threading.Event()
        errors: list[str] = []

        def value(i: int) -> bytes:
            unit = f"{i:08d}".encode()
            return unit * (value_size // len(unit))

        def writer(widx: int):
            c = runner.get_redis_client()
            i = widx
            while not stop.is_set():
                c.set(keys[i % len(keys)], value(i))
                i += 2

        def reader():
            c = runner.get_redis_client()
            while not stop.is_set():
                for k in keys:
                    v = c.get(k)
                    if v is None:
                        continue
                    assert isinstance(v, bytes)
                    if len(v) != len(value(0)) or v != v[:8] * (len(v) // 8):
                        errors.append(f"{k}: inconsistent value {v[:32]!r}...")
                        return

        threads = [threading.Thread(target=writer, args=(w,)) for w in range(2)]
        threads += [threading.Thread(target=reader) for _ in range(4)]
        for t in threads:
            t.start()
        time.sleep(5)
        stop.set()
        for t in threads:
            t.join()

        assert not errors, errors[0]
    finally:
        runner.stop(cleanup=True, kill=False)


def test_redis_hashes(hs5_redis: Hs5Runner):
    check_hashes(hs5_redis)


def check_hashes(hs5_redis: Hs5Runner):
    r = hs5_redis.get_redis_client()
    s3 = hs5_redis.get_s3_client()

    assert r.hset("h", mapping={"a": b"1", "b": b"2"}) == 2
    assert r.hset("h", "a", b"3") == 0
    assert r.hget("h", "a") == b"3"
    assert r.hget("h", "missing") is None
    assert r.hget("missing", "a") is None
    assert r.hmget("h", ["a", "b", "c"]) == [b"3", b"2", None]
    assert r.hgetall("h") == {b"a": b"3", b"b": b"2"}
    assert r.hlen("h") == 2
    assert r.hexists("h", "a")
    assert not r.hexists("h", "c")
    assert sorted(r.hkeys("h")) == [b"a", b"b"]
    assert sorted(r.hvals("h")) == [b"2", b"3"]
    assert r.hsetnx("h", "a", b"x") == 0
    assert r.hsetnx("h", "c", b"x") == 1
    assert r.hincrby("h", "n", 5) == 5
    assert r.hincrby("h", "n", -2) == 3
    with pytest.raises(redis.ResponseError, match="not an integer"):
        r.hincrby("h", "c", 1)
    assert r.type("h") == b"hash"
    assert (
        s3.head_object(Bucket=hs5_redis.redis_bucket(), Key="h")["ContentType"]
        == "application/cdmi-object"
    )
    assert r.hdel("h", "a", "b", "c", "n", "missing") == 4
    # Empty hashes are deleted
    assert r.exists("h") == 0

    r.set("s", b"v")
    with pytest.raises(redis.ResponseError, match="WRONGTYPE"):
        r.hset("s", "a", b"1")
    r.hset("h2", "a", b"1")
    with pytest.raises(redis.ResponseError, match="WRONGTYPE"):
        r.get("h2")
    with pytest.raises(redis.ResponseError, match="WRONGTYPE"):
        r.lpush("h2", b"x")
    assert set(r.scan_iter(_type="hash")) == {b"h2"}

    r2 = hs5_redis.get_redis_client(protocol=2)
    assert r2.hgetall("h2") == {b"a": b"1"}


def test_redis_sets(hs5_redis: Hs5Runner):
    check_sets(hs5_redis)


def check_sets(hs5_redis: Hs5Runner):
    r = hs5_redis.get_redis_client()
    assert r.sadd("s", b"a", b"b", b"a") == 2
    assert r.sadd("s", b"c") == 1
    assert r.smembers("s") == {b"a", b"b", b"c"}
    assert r.scard("s") == 3
    assert r.sismember("s", b"a")
    assert not r.sismember("s", b"x")
    assert r.smismember("s", [b"a", b"x"]) == [True, False]
    assert r.srem("s", b"a", b"x") == 1
    assert r.type("s") == b"set"
    assert r.srem("s", b"b", b"c") == 2
    assert r.exists("s") == 0
    assert r.smembers("missing") == set()

    r2 = hs5_redis.get_redis_client(protocol=2)
    r2.sadd("s2", b"x")
    assert r2.smembers("s2") == {b"x"}


def test_redis_sorted_sets(hs5_redis: Hs5Runner):
    check_sorted_sets(hs5_redis)


def check_sorted_sets(hs5_redis: Hs5Runner):
    r = hs5_redis.get_redis_client()
    assert r.zadd("z", {"a": 1, "b": 2.5, "c": 3}) == 3
    assert r.zadd("z", {"a": 0.5}) == 0
    assert r.zadd("z", {"a": 0.5, "d": 4}, ch=True) == 1
    assert r.zadd("z", {"a": 10}, nx=True) == 0
    assert r.zadd("z", {"e": 10}, xx=True) == 0
    assert r.zadd("z", {"a": 0.1}, gt=True) == 0
    assert r.zscore("z", "a") == 0.5
    assert r.zscore("z", "missing") is None
    assert r.zcard("z") == 4
    assert r.zcount("z", "-inf", "+inf") == 4
    assert r.zcount("z", "(1", 3) == 2
    assert r.zrange("z", 0, -1) == [b"a", b"b", b"c", b"d"]
    # RESP3: [member, score] pairs
    assert r.zrange("z", 0, 1, withscores=True) == [[b"a", 0.5], [b"b", 2.5]]
    assert r.zrange("z", 0, 0, desc=True) == [b"d"]
    assert r.zrangebyscore("z", 1, 3) == [b"b", b"c"]
    assert r.zrevrangebyscore("z", 3, 1) == [b"c", b"b"]
    assert r.zrangebyscore("z", "-inf", "+inf", start=1, num=2) == [b"b", b"c"]
    # As used by kombu (ZRANGE ... BYSCORE REV LIMIT ... WITHSCORES)
    assert r.zrange(
        "z", 3, 0, desc=True, byscore=True, offset=0, num=10, withscores=True
    ) == [[b"c", 3.0], [b"b", 2.5], [b"a", 0.5]]
    assert r.zincrby("z", 2, "a") == 2.5
    assert r.zrank("z", "c") == 2
    assert r.zrevrank("z", "c") == 1
    assert r.zadd("z", {"a": 1}, incr=True) == 3.5
    assert r.zrem("z", "a", "missing") == 1
    assert r.zremrangebyscore("z", 0, 3) == 2
    assert r.zrange("z", 0, -1) == [b"d"]
    assert r.type("z") == b"zset"
    assert r.zremrangebyrank("z", 0, -1) == 1
    assert r.exists("z") == 0

    r2 = hs5_redis.get_redis_client(protocol=2)
    r2.zadd("z2", {"x": 1.5, "y": float("inf")})
    assert r2.zrange("z2", 0, -1, withscores=True) == [
        (b"x", 1.5),
        (b"y", float("inf")),
    ]
    assert r2.zscore("z2", "x") == 1.5


def test_redis_expiry(hs5_redis: Hs5Runner):
    r = hs5_redis.get_redis_client()
    s3 = hs5_redis.get_s3_client()

    assert r.ttl("missing") == -2
    r.set("k", b"v")
    assert r.ttl("k") == -1
    assert r.expire("k", 100)
    assert 99 <= r.ttl("k") <= 100
    assert 99000 <= r.pttl("k") <= 100000
    assert not r.expire("k", 200, nx=True)
    assert r.expire("k", 200, gt=True)
    assert r.persist("k")
    assert r.ttl("k") == -1
    assert not r.persist("k")
    assert not r.expire("missing", 10)

    # SET clears the expiry, KEEPTTL keeps it
    r.set("k", b"v", ex=100)
    r.set("k", b"v2", keepttl=True)
    assert r.ttl("k") > 0
    r.set("k", b"v3")
    assert r.ttl("k") == -1
    # INCR keeps it
    r.set("c", b"1", px=100000)
    r.incr("c")
    assert r.ttl("c") > 0

    # Lazy expiry on access
    r.set("e", b"v", px=300)
    r.rpush("el", b"a")
    r.pexpire("el", 300)
    r.hset("eh", "a", b"1")
    r.pexpire("eh", 300)
    assert r.get("e") == b"v"
    time.sleep(0.5)
    assert r.get("e") is None
    assert r.exists("el") == 0
    assert r.hgetall("eh") == {}
    assert r.ttl("e") == -2

    # Background expiry (without access)
    r.setex("bg", 1, b"v")
    r.psetex("bg2", 200, b"v")
    time.sleep(2)
    bucket = hs5_redis.redis_bucket()
    keys = {o["Key"] for o in s3.list_objects_v2(Bucket=bucket).get("Contents", [])}
    assert "bg" not in keys and "bg2" not in keys
    assert set(r.keys("*")) == {b"k", b"c"}

    # Expiry in the past deletes the key
    r.set("p", b"v")
    assert r.expireat("p", 1)
    assert r.exists("p") == 0

    # DEL clears the expiry, so a new key does not inherit it
    r.set("d", b"v", ex=1)
    r.delete("d")
    r.set("d", b"v2")
    assert r.ttl("d") == -1

    # Pushes and pops keep the expiry of lists (stored in the list counter)
    r.rpush("tl", b"b")
    assert r.expire("tl", 100)
    r.lpush("tl", b"a")
    r.rpush("tl", b"c")
    assert 99 <= r.ttl("tl") <= 100
    assert r.lpop("tl") == b"a"
    assert r.rpop("tl") == b"c"
    assert 99 <= r.ttl("tl") <= 100
    assert r.llen("tl") == 1
    assert r.persist("tl")
    assert r.ttl("tl") == -1
    assert r.llen("tl") == 1
    r.pexpire("tl", 200)
    time.sleep(0.4)
    assert r.exists("tl") == 0

    # Popping the last element of a list removes the expiry
    r.rpush("pl", b"a")
    r.expire("pl", 1)
    assert r.lpop("pl") == b"a"
    r.rpush("pl", b"b")
    assert r.ttl("pl") == -1

    # Expiry survives restarts
    r.set("r", b"v", ex=100)
    r.set("r2", b"v", px=1500)
    hs5_redis.commit_storage(s3)
    hs5_redis.restart()
    r = hs5_redis.get_redis_client()
    assert 90 <= r.ttl("r") <= 100
    time.sleep(2)
    assert r.get("r2") is None

    # Keys starting with the reserved byte are rejected
    with pytest.raises(redis.ResponseError, match="reserved"):
        r.set(b"\xffkey", b"v")

    assert r.flushdb()
    assert r.ttl("r") == -2
    r.set("r", b"v")
    assert r.ttl("r") == -1


def test_redis_multi_exec(hs5_redis: Hs5Runner):
    r = hs5_redis.get_redis_client()
    pipe = r.pipeline(transaction=True)
    pipe.set("a", b"1").incr("a").rpush("l", b"x").get("a")
    assert pipe.execute() == [True, 2, 1, b"2"]

    # Queueing errors abort the transaction
    with pytest.raises(redis.ResponseError):
        pipe = r.pipeline(transaction=True)
        pipe.set("a", b"5")
        pipe.execute_command("NOSUCHCOMMAND")
        pipe.execute()
    assert r.get("a") == b"2"

    # Runtime errors are returned per command
    pipe = r.pipeline(transaction=True)
    pipe.set("s", b"x").lpush("s", b"y").get("s")
    res = pipe.execute(raise_on_error=False)
    assert res[0] is True
    assert isinstance(res[1], redis.ResponseError)
    assert res[2] == b"x"

    # WATCH
    with r.pipeline() as pipe:
        pipe.watch("w")
        r.set("w", b"changed")
        pipe.multi()
        pipe.set("w", b"mine")
        with pytest.raises(redis.WatchError):
            pipe.execute()
    assert r.get("w") == b"changed"

    with r.pipeline() as pipe:
        pipe.watch("w")
        assert pipe.get("w") == b"changed"
        pipe.multi()
        pipe.set("w", b"mine")
        assert pipe.execute() == [True]
    assert r.get("w") == b"mine"

    def incr_transaction(pipe):
        val = int(pipe.get("counter") or 0)
        pipe.multi()
        pipe.set("counter", str(val + 1))

    def worker():
        c = hs5_redis.get_redis_client()
        for _ in range(20):
            c.transaction(incr_transaction, "counter")

    threads = [threading.Thread(target=worker) for _ in range(4)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    assert r.get("counter") == b"80"

    # DISCARD
    r.execute_command("MULTI")
    r.execute_command("SET", "d", "1")
    assert r.execute_command("ECHO", "x") == b"QUEUED"
    assert r.execute_command("DISCARD")
    assert r.get("d") is None
    with pytest.raises(redis.ResponseError, match="without MULTI"):
        r.execute_command("EXEC")

    # Blocking commands don't block in transactions
    pipe = r.pipeline(transaction=True)
    pipe.blpop(["empty"], timeout=0)
    assert pipe.execute() == [None]


def test_redis_pubsub(hs5_redis: Hs5Runner):
    for protocol in [2, 3]:
        r = hs5_redis.get_redis_client(protocol=protocol)
        p = r.pubsub()
        p.subscribe("chan")
        p.psubscribe("pat.*")
        # Wait for the subscriptions
        for _ in range(2):
            msg = p.get_message(timeout=5)
            assert msg is not None and msg["type"] in ("subscribe", "psubscribe")

        assert r.publish("chan", b"hello") == 1
        assert r.publish("pat.x", b"world") == 1
        assert r.publish("other", b"none") == 0
        msg = p.get_message(timeout=5)
        assert msg is not None
        assert msg["type"] == "message"
        assert msg["channel"] == b"chan"
        assert msg["data"] == b"hello"
        msg = p.get_message(timeout=5)
        assert msg is not None
        assert msg["type"] == "pmessage"
        assert msg["pattern"] == b"pat.*"
        assert msg["channel"] == b"pat.x"
        assert msg["data"] == b"world"

        # PING in subscribed mode
        p.ping()
        msg = p.get_message(timeout=5)
        assert msg is not None and msg["type"] == "pong"

        p.unsubscribe("chan")
        p.punsubscribe("pat.*")
        p.get_message(timeout=1)
        p.get_message(timeout=1)
        assert r.publish("chan", b"hello") == 0
        p.close()

    # Subscriptions are removed when the connection is closed
    r = hs5_redis.get_redis_client()
    p = r.pubsub()
    p.subscribe("chan2")
    p.get_message(timeout=5)
    assert r.publish("chan2", b"x") == 1
    p.close()
    deadline = time.monotonic() + 5
    while r.publish("chan2", b"x") != 0 and time.monotonic() < deadline:
        time.sleep(0.05)
    assert r.publish("chan2", b"x") == 0

    # RESP2: other commands are not allowed in subscribed state
    conn = socket.create_connection(("127.0.0.1", hs5_redis.get_redis_port()))
    sock_file = conn.makefile("rb")
    password = hs5_redis.get_redis_client().connection_pool.connection_kwargs[
        "password"
    ]
    conn.sendall(f"AUTH root {password}\r\nSUBSCRIBE c\r\nGET a\r\n".encode())
    assert sock_file.readline() == b"+OK\r\n"
    # *3 $9 subscribe $1 c :1
    for _ in range(6):
        sock_file.readline()
    assert sock_file.readline().startswith(b"-ERR Can't execute 'get'")
    conn.close()


def test_redis_scripting(hs5_redis: Hs5Runner):
    r = hs5_redis.get_redis_client()
    assert r.eval("return 1", 0) == 1
    assert r.eval("return 'a'", 0) == b"a"
    assert r.eval("return {1, 'b', {2}}", 0) == [1, b"b", [2]]
    assert r.eval("return redis.status_reply('FINE')", 0) in (b"FINE", "FINE")
    assert r.eval("return KEYS[1] .. ARGV[1]", 1, "k", "v") == b"kv"
    assert r.eval("return nil", 0) is None
    assert r.eval("return false", 0) is None
    assert r.eval("return true", 0) == 1
    assert r.eval("return 3.9", 0) == 3

    assert r.eval("return redis.call('SET', KEYS[1], ARGV[1])", 1, "x", "1") in (
        b"OK",
        "OK",
    )
    assert r.eval("return redis.call('GET', KEYS[1])", 1, "x") == b"1"
    assert r.eval("return redis.call('GET', 'missing')", 0) is None
    assert r.eval("return redis.call('INCRBY', KEYS[1], 10)", 1, "x") == 11
    assert r.eval("return redis.call('TIME')[1] ~= nil", 0) == 1

    # Errors
    with pytest.raises(redis.ResponseError, match="WRONGTYPE"):
        r.eval("return redis.call('LPUSH', KEYS[1], 'a')", 1, "x")
    assert (
        r.eval(
            "local ok = redis.pcall('LPUSH', KEYS[1], 'a'); return ok['err'] ~= nil",
            1,
            "x",
        )
        == 1
    )
    with pytest.raises(redis.ResponseError, match="custom"):
        r.eval("return redis.error_reply('ERR custom')", 0)
    with pytest.raises(redis.ResponseError):
        r.eval("error('boom')", 0)
    with pytest.raises(redis.ResponseError, match="compiling"):
        r.eval("this is not lua", 0)
    with pytest.raises(redis.ResponseError, match="not allowed"):
        r.eval("return redis.call('MULTI')", 0)
    with pytest.raises(redis.ResponseError):
        r.eval("return os.time()", 0)
    # Globals don't leak between calls
    r.eval("leaked = 1", 0)
    assert r.eval("return leaked", 0) is None

    # Script cache
    script = r.register_script("return redis.call('GET', KEYS[1])")
    assert script(keys=["x"]) == b"11"
    sha = r.script_load("return 42")
    assert r.evalsha(sha, 0) == 42
    assert r.script_exists(sha, "0" * 40) == [True, False]
    with pytest.raises(redis.exceptions.NoScriptError):
        r.evalsha("0" * 40, 0)
    assert r.script_flush()
    assert r.script_exists(sha) == [False]

    # Scripts in a pipeline (redis-py loads them via SCRIPT EXISTS/LOAD)
    pipe = r.pipeline()
    script(keys=["x"], client=pipe)
    assert pipe.execute() == [b"11"]

    # Scripts are atomic
    incr_script = r.register_script(
        "local v = tonumber(redis.call('GET', KEYS[1]) or '0'); "
        "redis.call('SET', KEYS[1], tostring(v + 1)); return v + 1"
    )

    def worker():
        c = hs5_redis.get_redis_client()
        s = c.register_script(incr_script.script)
        for _ in range(25):
            s(keys=["atomic"])

    threads = [threading.Thread(target=worker) for _ in range(4)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    assert r.get("atomic") == b"100"

    # Endless scripts are aborted
    with pytest.raises(redis.ResponseError, match="timeout"):
        r.eval("while true do end", 0)
    assert r.ping()


# Like kombu's SWEEP_BINDINGS: accesses keys built from ARGV (undeclared)
SWEEP_SCRIPT = """
local overdue = redis.call('ZRANGEBYSCORE', KEYS[1], '-inf', ARGV[2])
for _, name in ipairs(overdue) do
    redis.call('DEL', ARGV[1] .. name)
    redis.call('ZREM', KEYS[1], name)
end
return #overdue
"""

UNDECLARED_INCR_SCRIPT = """
local v = tonumber(redis.call('GET', ARGV[1]) or '0')
redis.call('SET', ARGV[1], tostring(v + 1))
return v + 1
"""


def test_redis_scripting_undeclared_keys(hs5_redis: Hs5Runner):
    r = hs5_redis.get_redis_client()

    r.zadd("deadlines", {"ex1": 1, "ex2": 2, "ex3": 100})
    for name in ["ex1", "ex2", "ex3"]:
        r.sadd(f"table:{name}", b"member")
    sweep = r.register_script(SWEEP_SCRIPT)
    assert sweep(keys=["deadlines"], args=["table:", 50]) == 2
    assert r.exists("table:ex1", "table:ex2", "table:ex3") == 1
    assert r.zrange("deadlines", 0, -1) == [b"ex3"]
    # Known global script now
    assert sweep(keys=["deadlines"], args=["table:", 50]) == 0

    # Scripts reading undeclared keys first are atomic (run with the global lock)
    def worker():
        c = hs5_redis.get_redis_client()
        s = c.register_script(UNDECLARED_INCR_SCRIPT)
        for _ in range(25):
            s(args=["undeclared"])

    threads = [threading.Thread(target=worker) for _ in range(4)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    assert r.get("undeclared") == b"100"

    # Scripts accessing undeclared keys after writing fail (they cannot be run again)
    with pytest.raises(redis.ResponseError, match="undeclared"):
        r.eval(
            "redis.call('SET', KEYS[1], 'x'); return redis.call('GET', 'other')",
            1,
            "declared",
        )
    assert r.get("declared") == b"x"
    # ... unless the script is known to be global
    assert (
        r.eval(
            "redis.call('SET', KEYS[1], 'x'); return redis.call('GET', 'other')",
            1,
            "declared",
        )
        is None
    )

    # Catching the abort in the script does not execute it twice
    assert (
        r.eval(
            "pcall(redis.call, 'GET', 'undeclared2'); return redis.call('INCR', KEYS[1])",
            1,
            "once",
        )
        == 1
    )
    assert r.get("once") == b"1"

    # In transactions, scripts cannot be run again
    pipe = r.pipeline(transaction=True)
    pipe.set("t1", b"1")
    pipe.eval(
        "redis.call('SET', KEYS[1], 'y'); return redis.call('GET', 'other2')", 1, "t2"
    )
    res = pipe.execute(raise_on_error=False)
    assert res[0] is True
    assert isinstance(res[1], redis.ResponseError)
    # Global script is known now, so the transaction runs with the global lock
    pipe = r.pipeline(transaction=True)
    pipe.eval(
        "redis.call('SET', KEYS[1], 'y'); return redis.call('GET', 'other2')", 1, "t2"
    )
    assert pipe.execute() == [None]


def test_redis_scripting_always_global(tmpdir: Path, testrun_uid: str):
    loc = Path(tmpdir) / uuid.uuid4().hex
    loc.mkdir()
    runner = Hs5Runner(
        loc,
        data_file_size_limit_mb=100,
        testrun_uid=testrun_uid,
        with_redis=True,
        extra_args=["--redis_script_always_global"],
    )
    try:
        r = runner.get_redis_client()
        # Undeclared keys after writing are fine, as all scripts run exclusively
        assert (
            r.eval(
                "redis.call('SET', KEYS[1], 'x'); return redis.call('GET', KEYS[1] .. '2')",
                1,
                "a",
            )
            is None
        )
        r.set("a2", b"v")
        assert (
            r.eval(
                "redis.call('SET', KEYS[1], 'x'); return redis.call('GET', KEYS[1] .. '2')",
                1,
                "a",
            )
            == b"v"
        )
        pipe = r.pipeline(transaction=True)
        pipe.eval("return redis.call('GET', 'a2')", 0)
        assert pipe.execute() == [b"v"]
    finally:
        runner.stop(cleanup=True, kill=False)


def test_redis_large_collections(tmpdir: Path, testrun_uid: str):
    """Collections with more than 2 elements are stored with one entry per element"""
    loc = Path(tmpdir) / uuid.uuid4().hex
    loc.mkdir()
    runner = Hs5Runner(
        loc,
        data_file_size_limit_mb=100,
        testrun_uid=testrun_uid,
        with_redis=True,
        extra_args=["--redis_collection_inline_max_entries=2"],
    )
    try:
        r = runner.get_redis_client()
        s3 = runner.get_s3_client()
        bucket = runner.redis_bucket()

        for check in [check_hashes, check_sets, check_sorted_sets]:
            check(runner)
            assert r.flushdb()

        # Long fields/members (stored with their hash in the entry key)
        long1 = b"a" * 1000
        long2 = b"a" * 999 + b"b"
        r.hset("h", mapping={long1: b"1", long2: b"2", "short": b"3"})
        assert r.hget("h", long1) == b"1"
        assert r.hget("h", long2) == b"2"
        assert r.hgetall("h") == {long1: b"1", long2: b"2", b"short": b"3"}
        assert r.hdel("h", long1) == 1
        assert r.hget("h", long1) is None
        r.sadd("s", long1, long2, b"x")
        assert r.smembers("s") == {long1, long2, b"x"}
        assert r.sismember("s", long2)
        r.zadd("z", {long1: 1, long2: 2, "x": 3})
        assert r.zscore("z", long2) == 2
        assert r.zrange("z", 0, -1) == [long1, long2, b"x"]
        assert r.zrem("z", long1) == 1
        assert r.zcard("z") == 2

        # Negative/fractional scores are ordered correctly
        r.zadd("scores", {"a": -1.5, "b": 0, "c": -100, "d": 2.25, "e": float("-inf")})
        assert r.zrange("scores", 0, -1) == [b"e", b"c", b"a", b"b", b"d"]
        assert r.zrangebyscore("scores", "(-100", 0) == [b"a", b"b"]
        assert r.zrevrangebyscore("scores", "+inf", -1.5, start=1, num=2) == [
            b"b",
            b"a",
        ]
        assert r.zcount("scores", "-inf", "(0") == 3
        assert r.zremrangebyscore("scores", "-inf", -1) == 3
        assert r.zrange("scores", 0, -1) == [b"b", b"d"]

        # Concurrent modifications of fields (field level locks)
        def worker(i: int):
            c = runner.get_redis_client()
            for j in range(20):
                c.hset("conc", f"f{i}-{j}", b"v")
                c.hincrby("conc", "counter", 1)
                c.zadd("concz", {f"m{i}-{j}": j})
                c.zincrby("concz", 1, "counter")

        threads = [threading.Thread(target=worker, args=(i,)) for i in range(4)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        assert r.hlen("conc") == 81
        assert r.hget("conc", "counter") == b"80"
        assert r.zcard("concz") == 81
        assert r.zscore("concz", "counter") == 80

        # Reads of multiple elements are atomic with respect to field modifications
        r.hset("atomic", mapping={"a": b"0", "b": b"0", "c": b"x"})
        r.zadd("atomicz", {"a": 0, "b": 0, "c": 1e18})
        stop = threading.Event()
        errors: list[str] = []

        def writer(i: int):
            c = runner.get_redis_client()
            j = 0
            while not stop.is_set():
                v = f"{i}-{j}".encode()
                c.hset("atomic", mapping={"a": v, "b": v})
                c.zadd("atomicz", {"a": j, "b": j})
                j += 1

        def reader():
            c = runner.get_redis_client()
            while not stop.is_set():
                h = c.hgetall("atomic")
                if h[b"a"] != h[b"b"]:
                    errors.append(f"hash {h}")
                a, b = c.hmget("atomic", ["a", "b"])
                if a != b:
                    errors.append(f"hmget {a} {b}")
                z = dict(
                    (m, sc) for m, sc in c.zrange("atomicz", 0, 1, withscores=True)
                )
                if z.get(b"a") != z.get(b"b"):
                    errors.append(f"zset {z}")

        threads = [threading.Thread(target=writer, args=(i,)) for i in range(4)]
        threads += [threading.Thread(target=reader) for _ in range(4)]
        for t in threads:
            t.start()
        time.sleep(3)
        stop.set()
        for t in threads:
            t.join()
        assert errors == []

        # SET replaces large collections (their entries are deleted)
        r.set("conc", b"string")
        assert r.get("conc") == b"string"
        r.delete("conc")
        r.hset("conc", "new", b"1")
        assert r.hgetall("conc") == {b"new": b"1"}
        assert r.hlen("conc") == 1

        # Expiry deletes the entries
        r.sadd("exp", b"a", b"b", b"c")
        r.pexpire("exp", 200)
        time.sleep(0.5)
        assert r.exists("exp") == 0
        r.sadd("exp", b"x")
        assert r.smembers("exp") == {b"x"}

        # Entries left over after deleting the key via S3 don't come back
        r.sadd("s3del", b"a", b"b", b"c")
        s3.delete_object(Bucket=bucket, Key="s3del")
        assert r.exists("s3del") == 0
        r.sadd("s3del", b"x")
        assert r.smembers("s3del") == {b"x"}
        r.sadd("s3del", b"y", b"z")
        assert r.smembers("s3del") == {b"x", b"y", b"z"}
        assert r.scard("s3del") == 3

        # Removing all elements deletes the key
        r.hset("empty", mapping={"a": b"1", "b": b"2", "c": b"3"})
        assert r.hdel("empty", "a", "b", "c") == 3
        assert r.exists("empty") == 0

        # Persistence
        r.hset("p", mapping={"a": b"1", "b": b"2", "c": b"3"})
        runner.commit_storage(s3)
        runner.restart()
        r = runner.get_redis_client()
        assert r.hgetall("p") == {b"a": b"1", b"b": b"2", b"c": b"3"}
        assert r.hlen("p") == 3
        r.set("p", b"x")
        r.delete("p")
        r.hset("p", "z", b"1")
        assert r.hgetall("p") == {b"z": b"1"}

        # Listings in transactions see the modifications of the transaction
        pipe = r.pipeline(transaction=True)
        pipe.hset("txh", mapping={"a": b"1", "b": b"2", "c": b"3"})
        pipe.hgetall("txh")
        pipe.hdel("txh", "a", "b", "c")
        pipe.exists("txh")
        pipe.rpush("txl", b"x", b"y")
        pipe.lrange("txl", 0, -1)
        pipe.delete("txl")
        pipe.lrange("txl", 0, -1)
        assert pipe.execute() == [
            3,
            {b"a": b"1", b"b": b"2", b"c": b"3"},
            3,
            0,
            2,
            [b"x", b"y"],
            1,
            [],
        ]

        assert r.flushdb()
        assert r.keys("*") == []
    finally:
        runner.stop(cleanup=True, kill=False)
