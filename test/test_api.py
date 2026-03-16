from dataclasses import dataclass
from pathlib import Path
import pytest
import requests
from hs5_fixture import Hs5Runner, hs5
import boto3
from botocore.client import Config
from botocore.exceptions import ClientError
import hs5_api.models as hs5_models


@dataclass
class LoginData:
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

    authSes = resp["ses"]

    ses.headers.update({"Authorization": f"Bearer {authSes}"})

    return LoginData(
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

    response = login_data.req.post(url+"listUsers", json={})
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

    response = login_data.req.post(url+"listRoles", json={})
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
        "username": "test",
        "password": "test"
    })

    assert response.status_code == 401

    login_data = get_session(hs5)

    response = login_data.req.post(url+"addUser", json={
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
        "username": "newroot",
        "password": "newroot"
    })

    assert response.status_code == 200

    newroot_user = get_user_data(hs5, login_data, "newroot")

    response = login_data.req.post(
        url + "addAccessKey",
        json={
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

    bucketname = "testbucket-to-delete"

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


def test_add_user_for_bucket(hs5: Hs5Runner):
    admin_client = hs5.get_api_client_admin()

    admin_client.add_bucket(hs5_models.AddBucketParams(bucketName="accessbucket"))

    user_resp = admin_client.add_user(hs5_models.AddUserParams(username="bucketuser", password="bucketuser"))

    user_id = user_resp.id
 
    access_key_resp = admin_client.add_access_key(hs5_models.AddAccessKeyParams(userId=user_id))

    bucketuser_access_key = access_key_resp.access_key
    bucketuser_secret_key = access_key_resp.secret_key

    policy_resp = admin_client.add_policy(hs5_models.AddPolicyParams(
        policyName="bucketuser-policy",
        policyDocument="""
            {
                "Version": "2012-10-17",
                "Statement": [
                    {
                        "Effect": "Allow",
                        "Action": "*",
                        "Resource": ["arn:aws:s3:::accessbucket/*", "arn:aws:s3:::accessbucket"]
                    }
                ]
            }
            """
    ))

    policy_id = policy_resp.id

    role_resp = admin_client.add_role(hs5_models.AddRoleParams(
         roleName="bucketuser-role"))

    role_id = role_resp.id

    admin_client.add_role_policy(hs5_models.AddRolePolicyParams(
        roleId=role_id,
        policyId=policy_id
    ))

    admin_client.add_user_role(hs5_models.AddUserRoleParams(
        userId=user_id,
        roleId=role_id
    ))


    s3 = boto3.client(
        "s3",
        endpoint_url=hs5.get_url(),
        aws_access_key_id=bucketuser_access_key,
        aws_secret_access_key=bucketuser_secret_key,
        config=Config(signature_version="s3v4")
    )

    # Try to list buckets (should succeed)
    resp = s3.list_buckets()
    assert "Buckets" in resp
    assert ["accessbucket"] == [b.get("Name") for b in resp["Buckets"]]

    user_client = hs5.get_api_client("bucketuser", "bucketuser")

    list_resp = user_client.list(hs5_models.ListParams(path="/"))

    names = [o.name for o in list_resp.objects]

    assert names == ["accessbucket"]

    # Can upload, read and delete objects in the bucket

    s3.put_object(Bucket="accessbucket", Key="testkey", Body=b"testdata")
    obj = s3.get_object(Bucket="accessbucket", Key="testkey")
    data = obj["Body"].read()
    assert data == b"testdata"
    s3.delete_object(Bucket="accessbucket", Key="testkey")


@pytest.mark.parametrize("perms", ["", "r", "w", "d", "rw", "rd", "wd", "rwd"])
def test_simple_permissions(hs5: Hs5Runner, perms: str):
    admin_client = hs5.get_api_client_admin()

    admin_client.add_bucket(hs5_models.AddBucketParams(bucketName="perm-bucket"))

    admin_s3 = hs5.get_s3_client()
    admin_s3.put_object(Bucket="perm-bucket", Key="existingkey", Body=b"testdata")

    user_resp = admin_client.add_user(hs5_models.AddUserParams(username="permuser", password="permuser"))

    user_id = user_resp.id
 
    access_key_resp = admin_client.add_access_key(hs5_models.AddAccessKeyParams(userId=user_id))

    perms_str = list[str]()

    if "r" in perms:
        perms_str.append("read")
    if "w" in perms:
        perms_str.append("write")
    if "d" in perms:
        perms_str.append("delete")

    admin_client.add_bucket_permission(hs5_models.AddBucketPermissionParams(
        bucketName="perm-bucket",
        addBucketPermissions=perms_str,
        userId=user_id
    ))

    permuser_access_key = access_key_resp.access_key
    permuser_secret_key = access_key_resp.secret_key

    s3 = boto3.client(
        "s3",
        endpoint_url=hs5.get_url(),
        aws_access_key_id=permuser_access_key,
        aws_secret_access_key=permuser_secret_key,
        config=Config(signature_version="s3v4")
    )

    if "r" in perms:
        # Try to list buckets (should succeed)
        resp = s3.list_buckets()
        assert "Buckets" in resp
        assert ["perm-bucket"] == [b.get("Name") for b in resp["Buckets"]]

    if "r" in perms:
        # Try to read an object (should succeed)
        obj = s3.get_object(Bucket="perm-bucket", Key="existingkey")
        data = obj["Body"].read()
        assert data == b"testdata"
    else:
        # Try to read an object (should fail)
        try:
            s3.get_object(Bucket="perm-bucket", Key="existingkey")
            assert False, "Should not have permission to read"
        except ClientError as e:
            assert "Error" in e.response
            assert "Message" in e.response["Error"]
            assert e.response["Error"]["Message"] == "Access Denied"
            pass

    if "r" in perms:
        res = s3.list_objects_v2(Bucket="perm-bucket")
        assert "Contents" in res
        assert len(res["Contents"]) == 1
        assert "Key" in res["Contents"][0]
        assert res["Contents"][0]["Key"] == "existingkey"
    else:
        try:
            s3.list_objects_v2(Bucket="perm-bucket")
            assert False, "Should not have permission to list objects"
        except ClientError as e:
            assert "Error" in e.response
            assert "Message" in e.response["Error"]
            assert e.response["Error"]["Message"] == "Access Denied"
            pass
    

    if "w" in perms:
        # Try to upload an object (should succeed)
        s3.put_object(Bucket="perm-bucket", Key="testkey", Body=b"testdata")
    else:
        # Try to upload an object (should fail)
        try:
            s3.put_object(Bucket="perm-bucket", Key="testkey", Body=b"testdata")
            assert False, "Should not have permission to upload"
        except ClientError as e:
            assert "Error" in e.response
            assert "Message" in e.response["Error"]
            assert e.response["Error"]["Message"] == "Access Denied"
            pass

    if "d" in perms:
        # Try to delete an object (should succeed)
        s3.delete_object(Bucket="perm-bucket", Key="existingkey")
    else:
        # Try to delete an object (should fail)
        try:
            s3.delete_object(Bucket="perm-bucket", Key="existingkey")
            assert False, "Should not have permission to delete"
        except ClientError as e:
            assert "Error" in e.response
            assert "Message" in e.response["Error"]
            assert e.response["Error"]["Message"] == "Access Denied"
            pass