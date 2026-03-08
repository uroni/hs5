//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     ListBucketPermissionsResp.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <nlohmann/json.hpp>
#include "helper.hpp"

#include "BucketPermission.hpp"

namespace Api {
    using nlohmann::json;

    struct ListBucketPermissionsResp {
        std::vector<BucketPermission> bucketPermissions;
        bool isTruncated;
        std::string nextMarker;
    };
}
