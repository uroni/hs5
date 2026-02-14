/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "Policy.h"
#include "DbDao.h"
#include "Action.h"
#include <nlohmann/json.hpp>
#include <fmt/core.h>

using json = nlohmann::json;

Policy::Policy(const std::string& data)
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
            const auto actionId = actionFromStr(std::string(*itAction));
            if(actionId == Action::Unknown)
                throw PolicyParseError(fmt::format("Unknown action {} in stmt {}", std::string(*itAction), newStmt.sid));

            newStmt.actions.push_back(actionId);
        }
        else if(itAction->is_array())
        {
            for(auto val: *itAction)
            {
                if(!val.is_string())
                    throw PolicyParseError(fmt::format("Invalid object in action in stmt {}", newStmt.sid));

                const auto actionId = actionFromStr(std::string(val));
                if(actionId == Action::Unknown)
                    throw PolicyParseError(fmt::format("Unknown action {} in stmt {}", std::string(val), newStmt.sid));

                newStmt.actions.push_back(actionId);
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

Policy::Policy(std::vector<Statement> statements)
    : statements(std::move(statements))
{

}

Policy::Policy(int64_t policyId)
{
    DbDao dbDao;
    const auto stmts = dbDao.getPolicyStatements(policyId);
    for(const auto& stmt: stmts)
    {
        Statement newStmt;
        newStmt.sid = stmt.sid;
        newStmt.effect = stmt.effect == 0 ? Statement::Effect::Deny : Statement::Effect::Allow;

        newStmt.resources = dbDao.getPolicyStatementResources(stmt.id);

        const auto actions = dbDao.getPolicyStatementActions(stmt.id);
        newStmt.actions.reserve(actions.size());
        for(const auto& action: actions)
        {
            newStmt.actions.push_back(static_cast<Action>(action));
        }

        statements.emplace_back(std::move(newStmt));
    }
}

bool Policy::resourceMatch(const std::string_view policyResource, const std::string_view resource) const
{
    size_t p = 0, r = 0;
    size_t starIdx = std::string::npos;
    size_t matchIdx = 0;

    while (r < resource.size()) {
        if (p < policyResource.size() && (policyResource[p] == '?' || policyResource[p] == resource[r])) {
            p++;
            r++;
        } else if (p < policyResource.size() && policyResource[p] == '*') {
            starIdx = p;
            matchIdx = r;
            p++;
        } else if (starIdx != std::string::npos) {
            p = starIdx + 1;
            matchIdx++;
            r = matchIdx;
        } else {
            return false;
        }
    }

    while (p < policyResource.size() && policyResource[p] == '*') {
        p++;
    }

    return p == policyResource.size();
}

Policy::AccessCheckResult Policy::checkAccess(const Action action, const std::string_view resource) const
{
    auto ret = AccessCheckResult::NoMatch;
    for(const auto& stmt: statements)
    {
        if(std::find(stmt.actions.begin(), stmt.actions.end(), action) == stmt.actions.end()
           && std::find(stmt.actions.begin(), stmt.actions.end(), Action::AllActions) == stmt.actions.end())
            continue;

        bool hasResourceMatch = false;
        for(const auto& resPattern: stmt.resources)
        {
            if(resourceMatch(resPattern, resource))
            {
                hasResourceMatch = true;
                break;
            }
        }

        if(!hasResourceMatch)
            continue;

        if(stmt.effect == Statement::Effect::Allow)
            ret = AccessCheckResult::Allow;
        else
            return AccessCheckResult::Deny;
    }

    return ret;
}