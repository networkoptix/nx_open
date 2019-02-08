#include "access_rules.h"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>

#include <nx/utils/log/log.h>

#include "../stree/cdb_ns.h"

namespace nx::cloud::db {
namespace data {

std::string FieldRule::toString(const nx::utils::stree::ResourceNameSet& nameset) const
{
    return (required ? "+" : "-") +
        nameset.findResourceByID(dataFieldId).name.toStdString();
}

//-------------------------------------------------------------------------------------------------

ApiRequestRule::ApiRequestRule(const std::string& path):
    path(path)
{
}

ApiRequestRule::ApiRequestRule(const char* path) :
    path(path)
{
}

ApiRequestRule::ApiRequestRule(
    const std::string& path,
    const std::vector<FieldRule>& fieldRules)
    :
    path(path),
    m_fieldRules(fieldRules)
{
}

std::string ApiRequestRule::toString(
    const nx::utils::stree::ResourceNameSet& nameset) const
{
    std::string result = path;
    if (!m_fieldRules.empty())
        result += ";" + fieldRulesToString(nameset);
    return result;
}

bool ApiRequestRule::parse(
    const nx::utils::stree::ResourceNameSet& nameset,
    const std::string& str)
{
    using namespace boost::algorithm;

    if (str.empty())
        return false;

    std::vector<std::string> tokens;
    boost::algorithm::split(tokens, str, is_any_of(";"), token_compress_on);

    path = tokens.front();
    if (tokens.size() == 1)
        return true;

    return parseFieldRules(nameset, tokens[1]);
}

bool ApiRequestRule::match(
    const nx::utils::stree::AbstractResourceReader& requestAttributes) const
{
    if (m_fieldRules.empty())
        return true;

    int matchedFieldCount = 0;
    for (const auto& fieldRule : m_fieldRules)
    {
        if (fieldRule.required && requestAttributes.contains(fieldRule.dataFieldId))
            ++matchedFieldCount;
        else if (!fieldRule.required && !requestAttributes.contains(fieldRule.dataFieldId))
            ++matchedFieldCount;
    }

    return matchedFieldCount > 0; //< This is OR condition.
}

bool ApiRequestRule::parseFieldRules(
    const nx::utils::stree::ResourceNameSet& nameset,
    const std::string& str)
{
    using namespace boost::algorithm;

    if (str.empty())
        return true;

    std::vector<std::string> tokens;
    boost::algorithm::split(tokens, str, is_any_of(","), token_compress_on);
    for (const auto& token : tokens)
    {
        if (token.empty())
            continue;

        std::tuple<std::string /*fieldName*/, bool /*fieldRequired*/> fieldRuleData;
        if (token.front() == '+')
            fieldRuleData = std::make_tuple(token.substr(1), true);
        else if (token.front() == '-')
            fieldRuleData = std::make_tuple(token.substr(1), false);
        else
            fieldRuleData = std::make_tuple(token, true);

        const auto fieldDescription =
            nameset.findResourceByName(std::get<0>(fieldRuleData).c_str());
        if (fieldDescription.id < 0)
            continue;

        m_fieldRules.push_back({ fieldDescription.id, std::get<1>(fieldRuleData) });
    }

    return true;
}

std::string ApiRequestRule::fieldRulesToString(
    const nx::utils::stree::ResourceNameSet& nameset) const
{
    std::vector<std::string> rulesStrings;
    for (const auto& fieldRule : m_fieldRules)
        rulesStrings.push_back(fieldRule.toString(nameset));
    return boost::algorithm::join(rulesStrings, ",");
}

//-------------------------------------------------------------------------------------------------

bool RequestRuleGroup::empty() const
{
    return rules.empty();
}

bool RequestRuleGroup::match(
    const std::string& path,
    const nx::utils::stree::AbstractResourceReader& requestAttributes) const
{
    for (const auto& requestRule : rules)
    {
        if (path == requestRule.path)
            return requestRule.match(requestAttributes);
    }

    return false;
}

std::string RequestRuleGroup::toString(
    const nx::utils::stree::ResourceNameSet& nameset,
    const std::string& rulePrefix) const
{
    if (rules.empty())
        return std::string();

    std::vector<std::string> rulesStr;
    for (const auto& rule : rules)
        rulesStr.push_back(rule.toString(nameset));
    return rulePrefix + boost::algorithm::join(rulesStr, ":" + rulePrefix);
}

void RequestRuleGroup::add(const ApiRequestRule& rule)
{
    rules.push_back(rule);
}

//-------------------------------------------------------------------------------------------------

std::string AccessRestrictions::toString(
    const nx::utils::stree::ResourceNameSet& nameset) const
{
    std::string result = requestsAllowed.toString(nameset, "+");
    if (!result.empty() && !requestsDenied.empty())
        result += ":";
    result += requestsDenied.toString(nameset, "-");
    return result;
}

bool AccessRestrictions::parse(
    const nx::utils::stree::ResourceNameSet& nameset,
    const std::string& str)
{
    // TODO: #ak toString() and parse are not symmetric for a bit. Refactor!
    // TODO: #ak Use spirit?

    using namespace boost::algorithm;

    std::vector<std::string> allRequestsWithModifiers;
    boost::algorithm::split(
        allRequestsWithModifiers,
        str,
        is_any_of(":"),
        token_compress_on);
    for (std::string& requestWithModifier : allRequestsWithModifiers)
    {
        if (requestWithModifier.empty())
            continue;

        ApiRequestRule apiRequestRule;
        if (!apiRequestRule.parse(
            nameset,
            requestWithModifier.substr(1)))
        {
            NX_ASSERT(false, requestWithModifier.substr(1).c_str());
            continue;
        }

        if (requestWithModifier[0] == '+')
        {
            requestsAllowed.rules.push_back(std::move(apiRequestRule));
        }
        else if (requestWithModifier[0] == '-')
        {
            requestsDenied.rules.push_back(std::move(apiRequestRule));
        }
        else
        {
            NX_ASSERT(false, requestWithModifier.c_str());
            continue;
        }
    }

    return true;
}

bool AccessRestrictions::authorize(
    const nx::utils::stree::AbstractResourceReader& requestAttributes) const
{
    std::string requestPath;
    if (!requestAttributes.get(attr::requestPath, &requestPath))
        return false;

    if (!requestsAllowed.empty())
        return requestsAllowed.match(requestPath, requestAttributes);

    return !requestsDenied.match(requestPath, requestAttributes);
}

} // namespace data
} // namespace nx::cloud::db
