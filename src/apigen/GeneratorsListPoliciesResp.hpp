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

#include "ListPoliciesResp.hpp"
#include "Policy.hpp"

namespace Api {
    void from_json(const json & j, Policy & x);
    void to_json(json & j, const Policy & x);

    void from_json(const json & j, ListPoliciesResp & x);
    void to_json(json & j, const ListPoliciesResp & x);

    inline void from_json(const json & j, Policy& x) {
        x.document = j.at("document").get<std::string>();
        x.id = j.at("id").get<std::string>();
        x.name = j.at("name").get<std::string>();
    }

    inline void to_json(json & j, const Policy & x) {
        j = json::object();
        j["document"] = x.document;
        j["id"] = x.id;
        j["name"] = x.name;
    }

    inline void from_json(const json & j, ListPoliciesResp& x) {
        x.isTruncated = j.at("isTruncated").get<bool>();
        x.nextMarker = j.at("nextMarker").get<std::string>();
        x.policies = j.at("policies").get<std::vector<Policy>>();
    }

    inline void to_json(json & j, const ListPoliciesResp & x) {
        j = json::object();
        j["isTruncated"] = x.isTruncated;
        j["nextMarker"] = x.nextMarker;
        j["policies"] = x.policies;
    }
}
