#pragma once

#include <string>

enum class Action
{
    Unknown = 0,
    GetObject = 1,
    HeadObject = 2,
    PutObject = 3,
    PutObjectPart = 4,
    DeleteObject = 5,
    ListObjects = 6,
    CompleteMultipartUpload = 7,
    CreateBucket = 8,
    DeleteObjects = 9,
    DeleteBucket = 10,
    AddUser = 11,
    RemoveUser = 12,
    ListUsers = 13,
    AddAccessKey = 14,
    ListAccessKeys = 15,
    RemoveAccessKey = 16,
    AddPolicy = 17,
    RemovePolicy = 18,
    ListPolicies = 19,
    AddRole = 20,
    RemoveRole = 21,
    ListRoles = 22,
    AddUserRole = 23,
    RemoveUserRole = 24,
    ListUserRoles = 25,
    AddRolePolicy = 26,
    RemoveRolePolicy = 27,
    ListRolePolicies = 28,
    AllActions=29,
    ListBuckets=30,
    ListMultipartUploads=31,
    AbortMultipartUpload=32,
    CreateMultipartUpload=33,
    ChangePolicy=34,
    CreateUser=35,
    ChangePassword=36,
    ChangeOwnPassword=37,
    GetBucketLocation=38,
    CopyObject=39,
    UploadPartCopy=40
};

Action actionFromStr(const std::string_view action);