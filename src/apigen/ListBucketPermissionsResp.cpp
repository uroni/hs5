//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     ListBucketPermissionsResp.cpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <nlohmann/json.hpp>
#include "helper.hpp"

#include "Permission.hpp"
#include "BucketPermission.hpp"
#include "ListBucketPermissionsResp.hpp"
namespace Api {
}
