import os
import random
import threading

from botocore.exceptions import ClientError
import pytest
from hs5_fixture import Hs5Runner, hs5


def _list_all_versions(s3_client, bucket: str, key: str) -> tuple[list, list]:
    versions = []
    markers = []
    kwargs = {"Bucket": bucket, "Prefix": key}
    while True:
        res = s3_client.list_object_versions(**kwargs)
        versions.extend(v for v in res.get("Versions", []) if v["Key"] == key)
        markers.extend(m for m in res.get("DeleteMarkers", []) if m["Key"] == key)
        if not res["IsTruncated"]:
            return versions, markers
        kwargs["KeyMarker"] = res["NextKeyMarker"]
        kwargs["VersionIdMarker"] = res["NextVersionIdMarker"]


@pytest.mark.slow
@pytest.mark.parametrize("restart", [False, True])
def test_versioned_concurrent_put_delete_same_key(hs5: Hs5Runner, restart: bool):
    # Concurrent writes and deletes to the same key in a versioned bucket must
    # not lose versions or leave an inconsistent version list
    # (see versitygw issue #2200 / s3-tests
    # test_versioned_concurrent_object_create_concurrent_remove)
    bucket = hs5.testbucketname()
    key = "concurrent-versioned"
    s3_client = hs5.get_s3_client()
    s3_client.put_bucket_versioning(
        Bucket=bucket, VersioningConfiguration={"Status": "Enabled"}
    )

    num_threads = 12
    ops_per_thread = 25
    rounds = 3

    for round_idx in range(rounds):
        lock = threading.Lock()
        # version id -> body for every acknowledged object version
        written: dict[str, bytes] = {}
        # every body sent by put or multipart upload, acknowledged or not yet
        sent: set[bytes] = set()
        markers: set[str] = set()
        deleted_versions: set[str] = set()
        deleted_markers: set[str] = set()
        errors: list[BaseException] = []
        start = threading.Barrier(num_threads)

        def worker(tidx: int):
            client = hs5.get_s3_client()
            rnd = random.Random(round_idx * 1000 + tidx)
            start.wait()
            try:
                for i in range(ops_per_thread):
                    body = f"r{round_idx}-t{tidx}-op{i}-".encode() + os.urandom(
                        rnd.randint(0, 64)
                    )
                    with lock:
                        sent.add(body)
                    op = rnd.random()
                    if op < 0.45:
                        vid = client.put_object(Bucket=bucket, Key=key, Body=body)[
                            "VersionId"
                        ]
                        with lock:
                            assert vid not in written and vid not in markers
                            written[vid] = body
                    elif op < 0.6:
                        # Delete without version id creates a delete marker
                        res = client.delete_object(Bucket=bucket, Key=key)
                        if res.get("DeleteMarker"):
                            with lock:
                                assert res["VersionId"] not in written
                                assert res["VersionId"] not in markers
                                markers.add(res["VersionId"])
                    elif op < 0.8:
                        # Delete a specific, already acknowledged version or marker
                        with lock:
                            candidates = [
                                v for v in written if v not in deleted_versions
                            ] + [m for m in markers if m not in deleted_markers]
                            if not candidates:
                                continue
                            vid = rnd.choice(candidates)
                            if vid in written:
                                deleted_versions.add(vid)
                            else:
                                deleted_markers.add(vid)
                        res = client.delete_object(
                            Bucket=bucket, Key=key, VersionId=vid
                        )
                        assert res["VersionId"] == vid
                        assert bool(res.get("DeleteMarker")) == (vid in markers)
                    elif op < 0.9:
                        # Copy the current version onto the same key
                        try:
                            vid = client.copy_object(
                                Bucket=bucket,
                                Key=key,
                                CopySource={"Bucket": bucket, "Key": key},
                            )["VersionId"]
                        except ClientError as e:
                            # Latest may be a delete marker or deleted concurrently
                            assert (
                                e.response.get("Error", {}).get("Code") == "NoSuchKey"
                            ), e.response
                            continue
                        copied = client.get_object(
                            Bucket=bucket, Key=key, VersionId=vid
                        )["Body"].read()
                        with lock:
                            assert vid not in written and vid not in markers
                            assert copied in sent
                            written[vid] = copied
                    else:
                        upl = client.create_multipart_upload(Bucket=bucket, Key=key)
                        part = client.upload_part(
                            Bucket=bucket,
                            Key=key,
                            UploadId=upl["UploadId"],
                            PartNumber=1,
                            Body=body,
                        )
                        vid = client.complete_multipart_upload(
                            Bucket=bucket,
                            Key=key,
                            UploadId=upl["UploadId"],
                            MultipartUpload={
                                "Parts": [{"ETag": part["ETag"], "PartNumber": 1}]
                            },
                        )["VersionId"]
                        with lock:
                            assert vid not in written and vid not in markers
                            written[vid] = body
            except BaseException as e:
                with lock:
                    errors.append(e)
                raise

        threads = [
            threading.Thread(target=worker, args=(t,)) for t in range(num_threads)
        ]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        assert not errors, errors

        if restart:
            hs5.restart()
            s3_client = hs5.get_s3_client()

        expected_versions = set(written) - deleted_versions
        expected_markers = markers - deleted_markers

        versions, listed_markers = _list_all_versions(s3_client, bucket, key)
        listed_version_ids = [v["VersionId"] for v in versions]
        listed_marker_ids = [m["VersionId"] for m in listed_markers]
        assert len(listed_version_ids) == len(set(listed_version_ids))
        assert len(listed_marker_ids) == len(set(listed_marker_ids))
        assert set(listed_version_ids) == expected_versions
        assert set(listed_marker_ids) == expected_markers

        for v in versions:
            obj = s3_client.get_object(Bucket=bucket, Key=key, VersionId=v["VersionId"])
            assert obj["Body"].read() == written[v["VersionId"]]

        for vid in deleted_versions:
            with pytest.raises(ClientError):
                s3_client.head_object(Bucket=bucket, Key=key, VersionId=vid)

        # Plain GET returns an existing version or hits a delete marker
        try:
            obj = s3_client.get_object(Bucket=bucket, Key=key)
            assert obj["VersionId"] in expected_versions
            assert obj["Body"].read() == written[obj["VersionId"]]
        except ClientError as e:
            assert e.response.get("Error", {}).get("Code") == "NoSuchKey"
            assert expected_markers or not expected_versions

        # Concurrently remove all remaining versions and markers
        def remove(vid: str):
            hs5.get_s3_client().delete_object(Bucket=bucket, Key=key, VersionId=vid)

        remove_threads = [
            threading.Thread(target=remove, args=(vid,))
            for vid in listed_version_ids + listed_marker_ids
        ]
        for t in remove_threads:
            t.start()
        for t in remove_threads:
            t.join()

        versions, listed_markers = _list_all_versions(s3_client, bucket, key)
        assert not versions
        assert not listed_markers
        with pytest.raises(ClientError):
            s3_client.head_object(Bucket=bucket, Key=key)


@pytest.mark.slow
@pytest.mark.parametrize("versioned", [False, True])
def test_concurrent_delete_same_key(hs5: Hs5Runner, versioned: bool):
    # Deleting a key concurrently must answer 204 to every request, also to the ones
    # that find the key already gone (see versitygw issue #2571)
    bucket = hs5.testbucketname()
    s3_client = hs5.get_s3_client()
    if versioned:
        s3_client.put_bucket_versioning(
            Bucket=bucket, VersioningConfiguration={"Status": "Enabled"}
        )

    num_threads = 8
    clients = [hs5.get_s3_client() for _ in range(num_threads)]

    for trial in range(100):
        key = f"double-delete-{trial}"
        version_id = s3_client.put_object(Bucket=bucket, Key=key, Body=b"x" * 4096).get(
            "VersionId"
        )

        start = threading.Barrier(num_threads)
        results: list = [None] * num_threads

        def delete(idx: int):
            start.wait()
            try:
                res = clients[idx].delete_object(Bucket=bucket, Key=key)
                results[idx] = res["ResponseMetadata"]["HTTPStatusCode"]
            except ClientError as e:
                results[idx] = e.response.get("Error", {}).get("Code")

        threads = [
            threading.Thread(target=delete, args=(i,)) for i in range(num_threads)
        ]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        assert results == [204] * num_threads, results

        with pytest.raises(ClientError):
            s3_client.head_object(Bucket=bucket, Key=key)

        if versioned:
            # Every delete created a delete marker and the version is still there
            versions, markers = _list_all_versions(s3_client, bucket, key)
            assert [v["VersionId"] for v in versions] == [version_id]
            assert len(markers) == num_threads
