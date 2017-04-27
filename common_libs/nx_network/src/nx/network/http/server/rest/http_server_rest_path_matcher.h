#pragma once

#include <map>

#include <boost/optional.hpp>

#include <QtCore/QRegExp>
#include <QtCore/QString>

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
    bool add(const nx_http::StringType& pathTemplate, Mapped mapped)
    {
        MatchContext matchContext;
        matchContext.regex = convertToRegex(QString::fromUtf8(pathTemplate));
        matchContext.mapped = std::move(mapped);

        return m_restPathToMatchContext.emplace(
            pathTemplate,
            std::move(matchContext)).second;
    }

    boost::optional<const Mapped&> match(
        const nx_http::StringType& path,
        std::vector<nx_http::StringType>* pathParams) const
    {
        for (auto& matchContext: m_restPathToMatchContext)
        {
            if (!matchContext.second.regex.exactMatch(QString::fromUtf8(path)))
                continue;

            for (int i = 1; i <= matchContext.second.regex.captureCount(); ++i)
                pathParams->push_back(matchContext.second.regex.cap(i).toUtf8());

            return boost::optional<const Mapped&>(matchContext.second.mapped);
        }

        return boost::none;
    }

private:
    struct MatchContext
    {
        QRegExp regex;
        Mapped mapped;
    };

    /** REST path template, context */
    std::map<nx_http::StringType, MatchContext> m_restPathToMatchContext;

    QRegExp convertToRegex(const QString& pathTemplate)
    {
        const QRegExp replaceRestParams("\\{[0-9a-zA-Z]+\\}", Qt::CaseInsensitive);
        const QString replacement("([^/]+)");

        QString restPathMatchRegex;
        restPathMatchRegex += "^";
        restPathMatchRegex += pathTemplate;
        restPathMatchRegex += "$";
        restPathMatchRegex.replace(replaceRestParams, replacement);

        return QRegExp(std::move(restPathMatchRegex), Qt::CaseInsensitive);
    }
};

} // namespace rest
} // namespace server
} // namespace nx_http
