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
        static std::unordered_map<std::string, Herror> enumValues {
            {"access_denied", Herror::accessDenied},
            {"argon_encoding", Herror::argonEncoding},
            {"bucket_already_exists", Herror::bucketAlreadyExists},
            {"bucket_not_found", Herror::bucketNotFound},
            {"error_deleting_bucket", Herror::errorDeletingBucket},
            {"error_deleting_object", Herror::errorDeletingObject},
            {"error_listing_in_iteration", Herror::errorListingInIteration},
            {"error_starting_listing", Herror::errorStartingListing},
            {"internal_db_error", Herror::internalDbError},
            {"invalid_bucket_name", Herror::invalidBucketName},
            {"invalid_parameters", Herror::invalidParameters},
            {"invalid_path", Herror::invalidPath},
            {"invalid_policy_document", Herror::invalidPolicyDocument},
            {"no_such_function", Herror::noSuchFunction},
            {"password_wrong", Herror::passwordWrong},
            {"session_not_found", Herror::sessionNotFound},
            {"session_required", Herror::sessionRequired},
            {"unexpected_continuation_token", Herror::unexpectedContinuationToken},
            {"unknown_password_hashing", Herror::unknownPasswordHashing},
            {"user_already_exists", Herror::userAlreadyExists},
            {"user_not_found", Herror::userNotFound},
        };
        auto iter = enumValues.find(j.get<std::string>());
        if (iter != enumValues.end()) {
            x = iter->second;
        }
    }

    inline void to_json(json & j, const Herror & x) {
        switch (x) {
            case Herror::accessDenied: j = "access_denied"; break;
            case Herror::argonEncoding: j = "argon_encoding"; break;
            case Herror::bucketAlreadyExists: j = "bucket_already_exists"; break;
            case Herror::bucketNotFound: j = "bucket_not_found"; break;
            case Herror::errorDeletingBucket: j = "error_deleting_bucket"; break;
            case Herror::errorDeletingObject: j = "error_deleting_object"; break;
            case Herror::errorListingInIteration: j = "error_listing_in_iteration"; break;
            case Herror::errorStartingListing: j = "error_starting_listing"; break;
            case Herror::internalDbError: j = "internal_db_error"; break;
            case Herror::invalidBucketName: j = "invalid_bucket_name"; break;
            case Herror::invalidParameters: j = "invalid_parameters"; break;
            case Herror::invalidPath: j = "invalid_path"; break;
            case Herror::invalidPolicyDocument: j = "invalid_policy_document"; break;
            case Herror::noSuchFunction: j = "no_such_function"; break;
            case Herror::passwordWrong: j = "password_wrong"; break;
            case Herror::sessionNotFound: j = "session_not_found"; break;
            case Herror::sessionRequired: j = "session_required"; break;
            case Herror::unexpectedContinuationToken: j = "unexpected_continuation_token"; break;
            case Herror::unknownPasswordHashing: j = "unknown_password_hashing"; break;
            case Herror::userAlreadyExists: j = "user_already_exists"; break;
            case Herror::userNotFound: j = "user_not_found"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"Herror\": " + std::to_string(static_cast<int>(x)));
        }
    }
}
