#pragma once
#include <string>
#include <stdexcept>
#include <vector>
#include "s3handler.h"
#include "Action.h"

class PolicyParseError : std::runtime_error
{
public:
    PolicyParseError(std::string msg)
     : std::runtime_error(std::move(msg))
     {}
};

struct Statement
{
    std::string sid;

    enum class Effect
    {
        Deny = 0,
        Allow = 1,
    };

    Effect effect = Effect::Deny;

    std::vector<Action> actions;
    std::vector<std::string> resources;
};

class Policy
{
public:
    Policy(const std::string& data);
    Policy(std::vector<Statement> statements);
    Policy(int64_t policyId);

    enum class AccessCheckResult
    {
        Deny,
        Allow,
        NoMatch
    };    

    AccessCheckResult checkAccess(const Action action, const std::string_view resource) const;

private:

    bool resourceMatch(const std::string_view policyResource, const std::string_view resource) const;

    std::vector<Statement> statements;
};

