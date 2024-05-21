//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     HapiError.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <nlohmann/json.hpp>
#include "helper.hpp"

namespace Api {
    enum class Herror : int;
}

namespace Api {
    using nlohmann::json;

    struct HapiError {
        Herror herror;
        std::string msg;
    };
}
