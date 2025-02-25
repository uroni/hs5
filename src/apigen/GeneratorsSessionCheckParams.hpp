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

#include "SessionCheckParams.hpp"

namespace Api {
    void from_json(const json & j, SessionCheckParams & x);
    void to_json(json & j, const SessionCheckParams & x);

    inline void from_json(const json & j, SessionCheckParams& x) {
        x.ses = j.at("ses").get<std::string>();
    }

    inline void to_json(json & j, const SessionCheckParams & x) {
        j = json::object();
        j["ses"] = x.ses;
    }
}
