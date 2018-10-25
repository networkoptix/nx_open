#pragma once

#include <map>
#include <regex>
#include <string>

#include <boost/optional.hpp>

#include "../../http_types.h"

namespace nx {
namespace network {
namespace http {
namespace server {
namespace rest {

/**
 * Usage example:
 * - Register path "/account/{accountId}/systems".
 * - PathMatcher::match("/account/vpupkin/systems") matches to the registered path
 *   and adds "vpupkin" to pathParams.
 *
 * NOTE: Empty parameter is not matched.
 *   E.g., PathMatcher::match("/account//systems") returns boost::none.
 */
template<typename Mapped>
class PathMatcher
{
public:
    bool add(const std::string& pathTemplate, Mapped mapped)
    {
        MatchContext matchContext;
        matchContext.regex = convertToRegex(pathTemplate);
        matchContext.mapped = std::move(mapped);

        return m_restPathToMatchContext.emplace(
            pathTemplate,
            std::move(matchContext)).second;
    }

    boost::optional<const Mapped&> match(
        const std::string& path,
        std::vector<std::string>* pathParams) const
    {
        for (const auto& matchContext: m_restPathToMatchContext)
        {
            std::smatch matchResult;
            if (std::regex_search(path, matchResult, matchContext.second.regex))
            {
                std::vector<std::string> params;
                for (size_t i = 1; i < matchResult.size(); ++i)
                {
                    if (matchResult[i].length() == 0)
                        return boost::none;
                    params.push_back(matchResult[i]);
                }
                *pathParams = std::move(params);
                return boost::optional<const Mapped&>(matchContext.second.mapped);
            }
        }

        return boost::none;
    }

private:
    struct MatchContext
    {
        std::regex regex;
        Mapped mapped;
    };

    /** REST path template, context */
    std::map<std::string, MatchContext> m_restPathToMatchContext;

    std::regex convertToRegex(const std::string& pathTemplate)
    {
        const std::regex replaceRestParams("{[0-9a-zA-Z]*}", std::regex_constants::basic);
        const std::string replacement("\\([^/]*\\)");

        std::string restPathMatchRegex;
        restPathMatchRegex += "^";
        restPathMatchRegex += std::regex_replace(
            pathTemplate,
            replaceRestParams,
            replacement);
        restPathMatchRegex += "$";

        return std::regex(
            std::move(restPathMatchRegex),
            std::regex::icase | std::regex_constants::basic);
    }
};

} // namespace rest
} // namespace server
} // namespace nx
} // namespace network
} // namespace http
