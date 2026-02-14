//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     ListUsersResp.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <nlohmann/json.hpp>
#include "helper.hpp"

#include "User.hpp"

namespace Api {
    using nlohmann::json;

    struct ListUsersResp {
        bool isTruncated;
        std::string nextMarker;
        std::vector<User> users;
    };
}
