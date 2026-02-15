from dataclasses import dataclass
from pathlib import Path
import requests
from hs5_fixture import Hs5Runner, hs5
import boto3
from botocore.client import Config
from botocore.exceptions import ClientError

@dataclass
class LoginData:
    ses: str
    req: requests.Session
    access_key: str
    secret_access_key: str

def get_session(hs5: Hs5Runner) -> LoginData:
    url = hs5.get_api_url()

    ses = requests.session()

    response = ses.post(url+"login", json={
        "username": "root",
        "password": hs5.get_root_key()
    })

    assert response.status_code == 200
    resp = response.json()
    return LoginData(
        ses=resp["ses"],
        req=ses,
        access_key=resp.get("accessKey", ""),
        secret_access_key=resp.get("secretAccessKey", "")
    )


@dataclass
class UserData:
    id: str
    name: str

def get_user_data(hs5: Hs5Runner, login_data: LoginData, username: str) -> UserData:
    url = hs5.get_api_url()

    response = login_data.req.post(url+"listUsers", json={"ses": login_data.ses})
    assert response.status_code == 200
    users = response.json()
    user = list(filter(lambda u: u["username"] == username, users["users"]))[0]
    return UserData(
        id=user["id"],
        name=user["username"]
    )

@dataclass
class Role:
    id: str
    name: str

def get_role_data(hs5: Hs5Runner, login_data: LoginData, role_name: str) -> Role:
    url = hs5.get_api_url()

    response = login_data.req.post(url+"listRoles", json={"ses": login_data.ses})
    assert response.status_code == 200
    roles = response.json()
    role = list(filter(lambda r: r["roleName"] == role_name, roles["roles"]))[0]
    return Role(
        id=role["id"],
        name=role["roleName"]
    )

def test_login(hs5: Hs5Runner):
    get_session(hs5)

def test_adduser(hs5: Hs5Runner):
    url = hs5.get_api_url()

    ses1 = requests.session()

    response = ses1.post(url+"addUser", json={
        "ses": "foobar",
        "username": "test",
        "password": "test"
    })

    assert response.status_code == 401

    login_data = get_session(hs5)

    response = login_data.req.post(url+"addUser", json={
        "ses": login_data.ses,
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

def test_stats(hs5: Hs5Runner):
    hs5.get_stats()

def test_add_access_key(hs5: Hs5Runner):
    url = hs5.get_api_url()
    login_data = get_session(hs5)
    root_user = get_user_data(hs5, login_data, "root")

    s3_orig = hs5.get_s3_client()
    resp = s3_orig.list_buckets()
    assert "Buckets" in resp

    s3_ses = boto3.client(
        "s3",
        endpoint_url=hs5.get_url(),
        aws_access_key_id=login_data.access_key,
        aws_secret_access_key=login_data.secret_access_key,
        config=Config(signature_version="s3v4")
    )
    # Try to list buckets (should succeed)
    resp = s3_ses.list_buckets()
    assert "Buckets" in resp

    # Add an access key for the root user
    response = login_data.req.post(
        url + "addAccessKey",
        json={
            "ses": login_data.ses,
            "userId": root_user.id
        }
    )
    assert response.status_code == 200
    data = response.json()
    assert "accessKey" in data
    assert "secretKey" in data
    assert data["accessKey"]
    assert data["secretKey"]

    # Now test S3 request with the new access key
    

    s3 = boto3.client(
        "s3",
        endpoint_url=hs5.get_url(),
        aws_access_key_id=data["accessKey"],
        aws_secret_access_key=data["secretKey"],
        config=Config(signature_version="s3v4")
    )
    # Try to list buckets (should succeed)
    resp = s3.list_buckets()
    assert "Buckets" in resp

    resp = s3_ses.list_buckets()
    assert "Buckets" in resp

    resp = s3_orig.list_buckets()
    assert "Buckets" in resp

    assert hs5.testbucketname() in [b.get("Name") for b in resp["Buckets"]]


def test_add_new_root_user(hs5: Hs5Runner):
    url = hs5.get_api_url()
    login_data = get_session(hs5)

    response = login_data.req.post(url+"addUser", json={
        "ses": login_data.ses,
        "username": "newroot",
        "password": "newroot"
    })

    assert response.status_code == 200

    newroot_user = get_user_data(hs5, login_data, "newroot")

    response = login_data.req.post(
        url + "addAccessKey",
        json={
            "ses": login_data.ses,
            "userId": newroot_user.id
        }
    )
    assert response.status_code == 200
    data = response.json()
    assert "accessKey" in data
    assert "secretKey" in data
    assert data["accessKey"]
    assert data["secretKey"]

    newroot_access_key = data["accessKey"]
    newroot_secret_key = data["secretKey"]

    rootRole = get_role_data(hs5, login_data, "root")

    response = login_data.req.post(
        url + "addUserRole",
        json={
            "ses": login_data.ses,
            "userId": newroot_user.id,
            "roleId": rootRole.id
        }
    )
    assert response.status_code == 200

    s3 = boto3.client(
        "s3",
        endpoint_url=hs5.get_url(),
        aws_access_key_id=newroot_access_key,
        aws_secret_access_key=newroot_secret_key,
        config=Config(signature_version="s3v4")
    )
    # Try to list buckets (should succeed)
    resp = s3.list_buckets()
    assert "Buckets" in resp
    assert hs5.testbucketname() in [b.get("Name") for b in resp["Buckets"]]


def test_delete_bucket_via_api(hs5: Hs5Runner, tmp_path: Path):
    s3_client = hs5.get_s3_client()

    bucketname = "testbucket_to_delete"

    s3_client.create_bucket(Bucket=bucketname)

    bucket_info = s3_client.head_bucket(Bucket=bucketname)
    assert bucket_info["ResponseMetadata"]["HTTPStatusCode"] == 200

    from test_basic import add_objects
    add_objects(tmp_path, hs5, num_objects=10, object_data=b"abc", bucketname=bucketname)

    objs = s3_client.list_objects_v2(Bucket=bucketname)
    assert "Contents" in objs
    assert len(objs["Contents"]) == 10


    url = hs5.get_api_url()
    login_data = get_session(hs5)
    response = login_data.req.post(
        url + "deleteBucket",
        json={
            "ses": login_data.ses,
            "bucketName": bucketname
        }
    )
    assert response.status_code == 200

    # Check that bucket is deleted
    try:
        s3_client.head_bucket(Bucket=bucketname)
        assert False, "Bucket should be deleted"
    except ClientError as e:
        assert "Error" in e.response
        assert "Message" in e.response["Error"]
        assert e.response["Error"]["Message"] == "Not Found"
        pass

    # Check that listing objects in the deleted bucket returns an error
    try:
        s3_client.list_objects_v2(Bucket=bucketname)
        assert False, "Bucket should be deleted"
    except ClientError as e:
        assert "Error" in e.response
        assert "Message" in e.response["Error"]
        assert e.response["Error"]["Message"] == "The specified bucket does not exist"
        pass



