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

#include "AddPolicyParams.hpp"

namespace Api {
    void from_json(const json & j, AddPolicyParams & x);
    void to_json(json & j, const AddPolicyParams & x);

    inline void from_json(const json & j, AddPolicyParams& x) {
        x.policyDocument = j.at("policyDocument").get<std::string>();
        x.policyName = j.at("policyName").get<std::string>();
        x.ses = j.at("ses").get<std::string>();
    }

    inline void to_json(json & j, const AddPolicyParams & x) {
        j = json::object();
        j["policyDocument"] = x.policyDocument;
        j["policyName"] = x.policyName;
        j["ses"] = x.ses;
    }
}
