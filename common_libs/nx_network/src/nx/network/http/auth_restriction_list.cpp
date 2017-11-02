#include "auth_restriction_list.h"

namespace nx_http {

unsigned int AuthMethodRestrictionList::getAllowedAuthMethods(
    const nx_http::Request& request) const
{
    QString path = request.requestLine.url.path();
    unsigned int allowed =
        AuthMethod::cookie | AuthMethod::http | 
        AuthMethod::videowall | AuthMethod::urlQueryParam;

    QnMutexLocker lock(&m_mutex);
    for (const auto& rule: m_allowed)
    {
        if (rule.second.expression.exactMatch(path))
            allowed |= rule.second.method;
    }

    for (const auto& rule: m_denied)
    {
        if (rule.second.expression.exactMatch(path))
            allowed &= ~rule.second.method;
    }

    return allowed;
}

AuthMethodRestrictionList::Rule::Rule(const QString& expression, unsigned int method):
    expression(QLatin1String("/*") + expression + QLatin1String("/?")),
    method(method)
{
}

void AuthMethodRestrictionList::allow(
    const QString& pathMask,
    AuthMethod::Value method)
{
    QnMutexLocker lock(&m_mutex);
    m_allowed.emplace(pathMask, Rule(pathMask, method));
}

void AuthMethodRestrictionList::deny(
    const QString& pathMask,
    AuthMethod::Value method)
{
    QnMutexLocker lock(&m_mutex);
    m_denied.emplace(pathMask, Rule(pathMask, method));
}

} // namespace nx_http
