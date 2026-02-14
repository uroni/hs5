#include "Action.h"

Action actionFromStr(const std::string_view action)
{
    if(action == "s3:GetObject")
        return Action::GetObject;
    if(action == "s3:HeadObject")
        return Action::HeadObject;
    if(action == "s3:PutObject")
        return Action::PutObject;
    if(action == "s3:PutObjectPart")
        return Action::PutObjectPart;
    if(action == "s3:DeleteObject")
        return Action::DeleteObject;
    if(action == "s3:ListObjects")
        return Action::ListObjects;
    if(action == "s3:CompleteMultipartUpload")
        return Action::CompleteMultipartUpload;
    if(action == "s3:CreateBucket")
        return Action::CreateBucket;
    if(action == "s3:DeleteObjects")
        return Action::DeleteObjects;
    if(action == "s3:DeleteBucket")
        return Action::DeleteBucket;

    return Action::Unknown;
}