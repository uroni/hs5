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

#include "HapiError.hpp"
#include "Herror.hpp"

namespace Api {
    void from_json(const json & j, HapiError & x);
    void to_json(json & j, const HapiError & x);

    void from_json(const json & j, Herror & x);
    void to_json(json & j, const Herror & x);

    inline void from_json(const json & j, HapiError& x) {
        x.herror = j.at("herror").get<Herror>();
        x.msg = j.at("msg").get<std::string>();
    }

    inline void to_json(json & j, const HapiError & x) {
        j = json::object();
        j["herror"] = x.herror;
        j["msg"] = x.msg;
    }

    inline void from_json(const json & j, Herror & x) {
        if (j == "argon_encoding") x = Herror::argonEncoding;
        else if (j == "bucket_not_found") x = Herror::bucketNotFound;
        else if (j == "error_listing_in_iteration") x = Herror::errorListingInIteration;
        else if (j == "error_starting_listing") x = Herror::errorStartingListing;
        else if (j == "invalid_path") x = Herror::invalidPath;
        else if (j == "no_such_function") x = Herror::noSuchFunction;
        else if (j == "password_wrong") x = Herror::passwordWrong;
        else if (j == "session_not_found") x = Herror::sessionNotFound;
        else if (j == "session_required") x = Herror::sessionRequired;
        else if (j == "unexpected_continuation_token") x = Herror::unexpectedContinuationToken;
        else if (j == "unknown_password_hashing") x = Herror::unknownPasswordHashing;
        else if (j == "user_already_exists") x = Herror::userAlreadyExists;
        else if (j == "user_not_found") x = Herror::userNotFound;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const Herror & x) {
        switch (x) {
            case Herror::argonEncoding: j = "argon_encoding"; break;
            case Herror::bucketNotFound: j = "bucket_not_found"; break;
            case Herror::errorListingInIteration: j = "error_listing_in_iteration"; break;
            case Herror::errorStartingListing: j = "error_starting_listing"; break;
            case Herror::invalidPath: j = "invalid_path"; break;
            case Herror::noSuchFunction: j = "no_such_function"; break;
            case Herror::passwordWrong: j = "password_wrong"; break;
            case Herror::sessionNotFound: j = "session_not_found"; break;
            case Herror::sessionRequired: j = "session_required"; break;
            case Herror::unexpectedContinuationToken: j = "unexpected_continuation_token"; break;
            case Herror::unknownPasswordHashing: j = "unknown_password_hashing"; break;
            case Herror::userAlreadyExists: j = "user_already_exists"; break;
            case Herror::userNotFound: j = "user_not_found"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"[object Object]\": " + std::to_string(static_cast<int>(x)));
        }
    }
}
