//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     Error.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <nlohmann/json.hpp>
#include "helper.hpp"

namespace Api {
    using nlohmann::json;

    enum class Error : int { argonEncoding, passwordWrong, sessionNotFound, sessionRequired, unknownPasswordHashing, userAlreadyExists, userNotFound };
}
