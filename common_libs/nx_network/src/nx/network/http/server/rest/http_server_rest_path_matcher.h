#pragma once

#include <map>
#include <regex>
#include <string>

#include <boost/optional.hpp>

namespace nx_http {
namespace server {
namespace rest {

/**
 * Given registered path: "/account/{accountId}/systems".
 * When matching path "/account/vpupkin/systems".
 * Then "/account/{accountId}/systems" is found and pathParams contains "vpupkin".
 * Parameter cannot be empty. In that case path is not matched.
 */
template<typename Mapped>
class RestPathMatcher
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
                for (size_t i = 1; i < matchResult.size(); ++i)
                    pathParams->push_back(matchResult[i]);
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
        const std::regex replaceRestParams("\\{[\\w\\d]+\\}");

        std::string restPathMatchRegex;
        restPathMatchRegex += "^";
        restPathMatchRegex +=
            std::regex_replace(pathTemplate, replaceRestParams, "([\\w\\d\\-_.]+)");
        restPathMatchRegex += "$";

        return std::regex(std::move(restPathMatchRegex), std::regex::icase);
    }
};

} // namespace rest
} // namespace server
} // namespace nx_http
