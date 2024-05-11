#pragma once
#include <string>
#include <stdexcept>
#include <vector>

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
        Allow,
        Deny
    };

    Effect effect = Effect::Deny;

    std::vector<std::string> actions;
    std::vector<std::string> resources;
};

class Policy
{
public:
    Policy(std::string data);

    std::vector<Statement> statements;

private:
    std::string data;
};

