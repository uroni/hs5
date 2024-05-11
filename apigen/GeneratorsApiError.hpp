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

#include "ApiError.hpp"
#include "Error.hpp"

namespace Api {
    void from_json(const json & j, ApiError & x);
    void to_json(json & j, const ApiError & x);

    void from_json(const json & j, Error & x);
    void to_json(json & j, const Error & x);

    inline void from_json(const json & j, ApiError& x) {
        x.error = j.at("error").get<Error>();
        x.msg = j.at("msg").get<std::string>();
    }

    inline void to_json(json & j, const ApiError & x) {
        j = json::object();
        j["error"] = x.error;
        j["msg"] = x.msg;
    }

    inline void from_json(const json & j, Error & x) {
        if (j == "argon_encoding") x = Error::argonEncoding;
        else if (j == "password_wrong") x = Error::passwordWrong;
        else if (j == "session_not_found") x = Error::sessionNotFound;
        else if (j == "session_required") x = Error::sessionRequired;
        else if (j == "unknown_password_hashing") x = Error::unknownPasswordHashing;
        else if (j == "user_already_exists") x = Error::userAlreadyExists;
        else if (j == "user_not_found") x = Error::userNotFound;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const Error & x) {
        switch (x) {
            case Error::argonEncoding: j = "argon_encoding"; break;
            case Error::passwordWrong: j = "password_wrong"; break;
            case Error::sessionNotFound: j = "session_not_found"; break;
            case Error::sessionRequired: j = "session_required"; break;
            case Error::unknownPasswordHashing: j = "unknown_password_hashing"; break;
            case Error::userAlreadyExists: j = "user_already_exists"; break;
            case Error::userNotFound: j = "user_not_found"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"[object Object]\": " + std::to_string(static_cast<int>(x)));
        }
    }
}
