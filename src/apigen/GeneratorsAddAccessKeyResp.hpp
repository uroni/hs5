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

#include "AddAccessKeyResp.hpp"

namespace Api {
    void from_json(const json & j, AddAccessKeyResp & x);
    void to_json(json & j, const AddAccessKeyResp & x);

    inline void from_json(const json & j, AddAccessKeyResp& x) {
        x.accessKey = j.at("accessKey").get<std::string>();
        x.id = j.at("id").get<std::string>();
        x.secretKey = j.at("secretKey").get<std::string>();
    }

    inline void to_json(json & j, const AddAccessKeyResp & x) {
        j = json::object();
        j["accessKey"] = x.accessKey;
        j["id"] = x.id;
        j["secretKey"] = x.secretKey;
    }
}
