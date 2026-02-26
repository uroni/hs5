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

#include "StatsResp.hpp"

namespace Api {
    void from_json(const json & j, StatsResp & x);
    void to_json(json & j, const StatsResp & x);

    inline void from_json(const json & j, StatsResp& x) {
        x.freeSpace = j.at("free_space").get<int64_t>();
        x.size = j.at("size").get<int64_t>();
        x.sizeFull = j.at("size_full").get<int64_t>();
        x.used = j.at("used").get<int64_t>();
    }

    inline void to_json(json & j, const StatsResp & x) {
        j = json::object();
        j["free_space"] = x.freeSpace;
        j["size"] = x.size;
        j["size_full"] = x.sizeFull;
        j["used"] = x.used;
    }
}
