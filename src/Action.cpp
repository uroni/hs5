/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "Action.h"
#include <string_view>

Action actionFromStr(const std::string_view action)
{
    if(action == "s3:GetObject") return Action::GetObject;
    if(action == "s3:HeadObject") return Action::HeadObject;
    if(action == "s3:PutObject") return Action::PutObject;
    if(action == "s3:PutObjectPart") return Action::PutObjectPart;
    if(action == "s3:DeleteObject") return Action::DeleteObject;
    if(action == "s3:ListObjects") return Action::ListObjects;
    if(action == "s3:CompleteMultipartUpload") return Action::CompleteMultipartUpload;
    if(action == "s3:CreateBucket") return Action::CreateBucket;
    if(action == "s3:DeleteObjects") return Action::DeleteObjects;
    if(action == "s3:DeleteBucket") return Action::DeleteBucket;
    if(action == "hs5:AddUser") return Action::AddUser;
    if(action == "hs5:RemoveUser") return Action::RemoveUser;
    if(action == "hs5:ListUsers") return Action::ListUsers;
    if(action == "hs5:AddAccessKey") return Action::AddAccessKey;
    if(action == "hs5:ListAccessKeys") return Action::ListAccessKeys;
    if(action == "hs5:RemoveAccessKey") return Action::RemoveAccessKey;
    if(action == "hs5:AddPolicy") return Action::AddPolicy;
    if(action == "hs5:RemovePolicy") return Action::RemovePolicy;
    if(action == "hs5:ListPolicies") return Action::ListPolicies;
    if(action == "hs5:AddRole") return Action::AddRole;
    if(action == "hs5:RemoveRole") return Action::RemoveRole;
    if(action == "hs5:ListRoles") return Action::ListRoles;
    if(action == "hs5:AddUserRole") return Action::AddUserRole;
    if(action == "hs5:RemoveUserRole") return Action::RemoveUserRole;
    if(action == "hs5:ListUserRoles") return Action::ListUserRoles;
    if(action == "hs5:AddRolePolicy") return Action::AddRolePolicy;
    if(action == "hs5:RemoveRolePolicy") return Action::RemoveRolePolicy;
    if(action == "hs5:ListRolePolicies") return Action::ListRolePolicies;
    if(action == "hs5:AllActions" || action == "*") return Action::AllActions;
    if(action == "s3:ListBuckets") return Action::ListBuckets;
    if(action == "s3:ListMultipartUploads") return Action::ListMultipartUploads;
    if(action == "s3:AbortMultipartUpload") return Action::AbortMultipartUpload;
    if(action == "s3:CreateMultipartUpload") return Action::CreateMultipartUpload;
    if(action == "hs5:ChangePolicy") return Action::ChangePolicy;
    if(action == "hs5:CreateUser") return Action::CreateUser;
    if(action == "hs5:ChangePassword") return Action::ChangePassword;
    if(action == "hs5:ChangeOwnPassword") return Action::ChangeOwnPassword;
    return Action::Unknown;
}

std::string actionToStr(const Action action)
{
    switch(action)
    {
        case Action::GetObject: return "s3:GetObject";
        case Action::HeadObject: return "s3:HeadObject";
        case Action::PutObject: return "s3:PutObject";
        case Action::PutObjectPart: return "s3:PutObjectPart";
        case Action::DeleteObject: return "s3:DeleteObject";
        case Action::ListObjects: return "s3:ListObjects";
        case Action::CompleteMultipartUpload: return "s3:CompleteMultipartUpload";
        case Action::CreateBucket: return "s3:CreateBucket";
        case Action::DeleteObjects: return "s3:DeleteObjects";
        case Action::DeleteBucket: return "s3:DeleteBucket";
        case Action::AddUser: return "hs5:AddUser";
        case Action::RemoveUser: return "hs5:RemoveUser";
        case Action::ListUsers: return "hs5:ListUsers";
        case Action::AddAccessKey: return "hs5:AddAccessKey";
        case Action::ListAccessKeys: return "hs5:ListAccessKeys";
        case Action::RemoveAccessKey: return "hs5:RemoveAccessKey";
        case Action::AddPolicy: return "hs5:AddPolicy";
        case Action::RemovePolicy: return "hs5:RemovePolicy";
        case Action::ListPolicies: return "hs5:ListPolicies";
        case Action::AddRole: return "hs5:AddRole";
        case Action::RemoveRole: return "hs5:RemoveRole";
        case Action::ListRoles: return "hs5:ListRoles";
        case Action::AddUserRole: return "hs5:AddUserRole";
        case Action::RemoveUserRole: return "hs5:RemoveUserRole";
        case Action::ListUserRoles: return "hs5:ListUserRoles";
        case Action::AddRolePolicy:return  "hs5:AddRolePolicy";
        case Action::RemoveRolePolicy:return  "hs5:RemoveRolePolicy";
        case Action::ListRolePolicies:return  "hs5:ListRolePolicies";
        case Action::AllActions:return  "*";
        case Action::ListBuckets:return  "s3:ListBuckets"; 
    }
    return "";
}