/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "Policy.h"

#include <nlohmann/json.hpp>
#include <fmt/core.h>

using json = nlohmann::json;

Policy::Policy(std::string data)
 : data(std::move(data))
{
    json j;

    try
    {
        j = json::parse(data);
    }
    catch(json::parse_error& e)
    {
        throw PolicyParseError(fmt::format("Error parsing JSON: {}", e.what()));
    }

    if(!j.is_object())
        throw PolicyParseError("Root item is not an object");

    auto itVersion = j.find("Version");
    if(itVersion==j.end())
        throw PolicyParseError("Version not found");

    if(!itVersion->is_string())
        throw PolicyParseError("Version is not a string");

    if(*itVersion != "2012-10-17")
        throw PolicyParseError(fmt::format("Unsupported policy version: {}", std::string(*itVersion)));

    auto itStatements = j.find("Statement");
    if(!itStatements->is_array())
        throw PolicyParseError("Statements array not found");

    for(auto stmt: *itStatements)
    {
        Statement newStmt;
        newStmt.sid = stmt.value("Sid", "");
        auto effect = stmt.value("Effect", "");

        if(effect == "Allow")
            newStmt.effect = Statement::Effect::Allow;
        else if(effect == "Deny")
            newStmt.effect = Statement::Effect::Deny;
        else
            throw PolicyParseError(fmt::format("Invalid Effect value {} in stmt {}", effect, newStmt.sid));

        auto itAction = stmt.find("Action");
        if(itAction==stmt.end())
            throw PolicyParseError(fmt::format("Statement {} without Action", newStmt.sid));

        if(itAction->is_string())
        {
            newStmt.actions.push_back(*itAction);
        }
        else if(itAction->is_array())
        {
            for(auto val: *itAction)
            {
                if(!val.is_string())
                    throw PolicyParseError(fmt::format("Invalid object in action in stmt {}", newStmt.sid));

                newStmt.actions.push_back(val);
            }
        }
        else
        {
            throw PolicyParseError(fmt::format("Statement {} Action is not string or array", newStmt.sid));
        }

        auto itResource = stmt.find("Resource");
        if(itResource==stmt.end())
            throw PolicyParseError(fmt::format("Statement {} without Resource", newStmt.sid));

        if(itResource->is_string())
        {
            newStmt.resources.push_back(*itResource);
        }
        else if(itResource->is_array())
        {
            for(auto val: *itResource)
            {
                if(!val.is_string())
                    throw PolicyParseError(fmt::format("Invalid object in Resource in stmt {}", newStmt.sid));

                newStmt.resources.push_back(val);
            }
        }
        else
        {
            throw PolicyParseError(fmt::format("Statement {} Resource is not string or array", newStmt.sid));
        }

        statements.emplace_back(std::move(newStmt));
    }
}

