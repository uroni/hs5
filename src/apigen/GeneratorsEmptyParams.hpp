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

#include "EmptyParams.hpp"

namespace Api {
    void from_json(const json & j, EmptyParams & x);
    void to_json(json & j, const EmptyParams & x);

    inline void from_json(const json & j, EmptyParams& x) {
        x.ses = j.at("ses").get<std::string>();
    }

    inline void to_json(json & j, const EmptyParams & x) {
        j = json::object();
        j["ses"] = x.ses;
    }
}
