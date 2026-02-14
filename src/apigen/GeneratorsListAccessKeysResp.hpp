//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     Generators.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <nlohmann/json.hpp>
#include "helper.hpp"

#include "ListAccessKeysResp.hpp"
#include "AccessKey.hpp"

namespace Api {
    void from_json(const json & j, AccessKey & x);
    void to_json(json & j, const AccessKey & x);

    void from_json(const json & j, ListAccessKeysResp & x);
    void to_json(json & j, const ListAccessKeysResp & x);

    inline void from_json(const json & j, AccessKey& x) {
        x.accessKeyId = j.at("accessKeyId").get<std::string>();
        x.id = j.at("id").get<std::string>();
    }

    inline void to_json(json & j, const AccessKey & x) {
        j = json::object();
        j["accessKeyId"] = x.accessKeyId;
        j["id"] = x.id;
    }

    inline void from_json(const json & j, ListAccessKeysResp& x) {
        x.accessKeys = j.at("accessKeys").get<std::vector<AccessKey>>();
        x.isTruncated = j.at("isTruncated").get<bool>();
        x.nextMarker = j.at("nextMarker").get<std::string>();
    }

    inline void to_json(json & j, const ListAccessKeysResp & x) {
        j = json::object();
        j["accessKeys"] = x.accessKeys;
        j["isTruncated"] = x.isTruncated;
        j["nextMarker"] = x.nextMarker;
    }
}
