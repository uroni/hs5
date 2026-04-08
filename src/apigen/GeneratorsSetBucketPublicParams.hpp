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

#include "SetBucketPublicParams.hpp"
#include "Public.hpp"

namespace Api {
    void from_json(const json & j, SetBucketPublicParams & x);
    void to_json(json & j, const SetBucketPublicParams & x);

    void from_json(const json & j, Public & x);
    void to_json(json & j, const Public & x);

    inline void from_json(const json & j, SetBucketPublicParams& x) {
        x.bucketName = j.at("bucketName").get<std::string>();
        x.setBucketPublicParamsPublic = j.at("public").get<std::vector<Public>>();
    }

    inline void to_json(json & j, const SetBucketPublicParams & x) {
        j = json::object();
        j["bucketName"] = x.bucketName;
        j["public"] = x.setBucketPublicParamsPublic;
    }

    inline void from_json(const json & j, Public & x) {
        if (j == "delete") x = Public::publicDelete;
        else if (j == "read") x = Public::read;
        else if (j == "write") x = Public::write;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const Public & x) {
        switch (x) {
            case Public::publicDelete: j = "delete"; break;
            case Public::read: j = "read"; break;
            case Public::write: j = "write"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"Public\": " + std::to_string(static_cast<int>(x)));
        }
    }
}
