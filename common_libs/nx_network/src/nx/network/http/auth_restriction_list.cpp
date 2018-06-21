#include "auth_restriction_list.h"

namespace nx {
namespace network {
namespace http {

const AuthMethod::Values AuthMethodRestrictionList::kDefaults =
    AuthMethod::sessionKey | AuthMethod::cookie | AuthMethod::http |
    AuthMethod::videowallHeader | AuthMethod::urlQueryDigest;

AuthMethod::Values AuthMethodRestrictionList::getAllowedAuthMethods(
    const nx::network::http::Request& request) const
{
    QString path = request.requestLine.url.path();
    AuthMethod::Values allowed = kDefaults;

    QnMutexLocker lock(&m_mutex);
    for (const auto& rule: m_allowed)
    {
        if (rule.second.expression.exactMatch(path))
            allowed |= rule.second.methods;
    }

    for (const auto& rule: m_denied)
    {
        if (rule.second.expression.exactMatch(path))
            allowed &= ~rule.second.methods;
    }

    return allowed;
}

AuthMethodRestrictionList::Rule::Rule(const QString& expression, AuthMethod::Values methods):
    expression(QLatin1String("/*") + expression + QLatin1String("/?")),
    methods(methods)
{
}

void AuthMethodRestrictionList::allow(const QString& pathMask, AuthMethod::Values method)
{
    QnMutexLocker lock(&m_mutex);
    m_allowed.emplace(pathMask, Rule(pathMask, method));
}

void AuthMethodRestrictionList::deny(const QString& pathMask, AuthMethod::Values method)
{
    QnMutexLocker lock(&m_mutex);
    m_denied.emplace(pathMask, Rule(pathMask, method));
}

} // namespace nx
} // namespace network
} // namespace http
