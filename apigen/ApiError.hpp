//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     ApiError.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <nlohmann/json.hpp>
#include "helper.hpp"

namespace Api {
    enum class Error : int;
}

namespace Api {
    using nlohmann::json;

    struct ApiError {
        Error error;
        std::string msg;
    };
}
