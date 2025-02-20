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

#include "ListParams.hpp"

namespace Api {
    void from_json(const json & j, ListParams & x);
    void to_json(json & j, const ListParams & x);

    inline void from_json(const json & j, ListParams& x) {
        x.continuationToken = get_stack_optional<std::string>(j, "continuationToken");
        x.path = j.at("path").get<std::string>();
        x.ses = j.at("ses").get<std::string>();
    }

    inline void to_json(json & j, const ListParams & x) {
        j = json::object();
        j["continuationToken"] = x.continuationToken;
        j["path"] = x.path;
        j["ses"] = x.ses;
    }
}
