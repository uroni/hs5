//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     Herror.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <nlohmann/json.hpp>
#include "helper.hpp"

namespace Api {
    using nlohmann::json;

    enum class Herror : int { accessDenied, argonEncoding, bucketAlreadyExists, bucketNotFound, errorDeletingBucket, errorDeletingObject, errorListingInIteration, errorStartingListing, internalDbError, invalidBucketName, invalidParameters, invalidPath, invalidPolicyDocument, noSuchFunction, passwordWrong, sessionNotFound, sessionRequired, unexpectedContinuationToken, unknownPasswordHashing, userAlreadyExists, userNotFound };
}
