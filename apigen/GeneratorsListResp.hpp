//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     Generators.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <optional>
#include <nlohmann/json.hpp>
#include "helper.hpp"

#include "ListResp.hpp"
#include "Object.hpp"

namespace Api {
    void from_json(const json & j, Object & x);
    void to_json(json & j, const Object & x);

    void from_json(const json & j, ListResp & x);
    void to_json(json & j, const ListResp & x);

    inline void from_json(const json & j, Object& x) {
        x.name = j.at("name").get<std::string>();
        x.size = get_stack_optional<int64_t>(j, "size");
        x.type = j.at("type").get<int64_t>();
    }

    inline void to_json(json & j, const Object & x) {
        j = json::object();
        j["name"] = x.name;
        j["size"] = x.size;
        j["type"] = x.type;
    }

    inline void from_json(const json & j, ListResp& x) {
        x.isTruncated = j.at("isTruncated").get<bool>();
        x.nextMarker = j.at("nextMarker").get<std::string>();
        x.objects = j.at("objects").get<std::vector<Object>>();
    }

    inline void to_json(json & j, const ListResp & x) {
        j = json::object();
        j["isTruncated"] = x.isTruncated;
        j["nextMarker"] = x.nextMarker;
        j["objects"] = x.objects;
    }
}
