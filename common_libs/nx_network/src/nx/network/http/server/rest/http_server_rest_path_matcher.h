#pragma once

#include <map>

#include <boost/optional.hpp>

#include <QtCore/QRegExp>
#include <QtCore/QString>

#include <nx/utils/thread/mutex.h>

#include "../../http_types.h"

namespace nx_http {
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
        QnMutexLocker lock(&m_mutex);

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
    mutable QnMutex m_mutex;

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
