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
    bool add(const std::string& pathTemplate, Mapped mapped)
    {
        MatchContext matchContext;
        matchContext.regex = convertToRegex(QString::fromStdString(pathTemplate));
        matchContext.mapped = std::move(mapped);

        return m_restPathToMatchContext.emplace(
            pathTemplate,
            std::move(matchContext)).second;
    }

    boost::optional<const Mapped&> match(
        const std::string& path,
        std::vector<std::string>* pathParams) const
    {
        for (auto& matchContext: m_restPathToMatchContext)
        {
            auto str = QString::fromStdString(path);

            int pos = 0;
            while ((pos = matchContext.second.regex.indexIn(str, pos)) != -1)
            {
                pathParams->push_back(matchContext.second.regex.cap(1).toStdString());
                pos += matchContext.second.regex.matchedLength();
            }

            if (pathParams->empty())
                continue;

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
    std::map<std::string, MatchContext> m_restPathToMatchContext;

    QRegExp convertToRegex(const QString& pathTemplate)
    {
        const QRegExp replaceRestParams("\\{[0-9a-zA-Z]+\\}", Qt::CaseInsensitive);
        const QString replacement("([0-9a-zA-Z_.-]+)");

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
