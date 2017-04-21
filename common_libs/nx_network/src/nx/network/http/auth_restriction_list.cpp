#include "auth_restriction_list.h"

#include <nx/utils/match/wildcard.h>

namespace nx_http {

unsigned int AuthMethodRestrictionList::getAllowedAuthMethods(
    const nx_http::Request& request) const
{
    QString path = request.requestLine.url.path();
    // TODO: #ak Replace mid and chop with single midRef call.
    while (path.startsWith(lit("//")))
        path = path.mid(1);
    while (path.endsWith(L'/'))
        path.chop(1);

    unsigned int allowed =
        AuthMethod::cookie | AuthMethod::http | 
        AuthMethod::videowall | AuthMethod::urlQueryParam;
    for (std::pair<QString, unsigned int> allowRule: m_allowed)
    {
        if (!wildcardMatch(allowRule.first, path))
            continue;
        allowed |= allowRule.second;
    }

    for (std::pair<QString, unsigned int> denyRule: m_denied)
    {
        if (!wildcardMatch(denyRule.first, path))
            continue;
        allowed &= ~denyRule.second;
    }

    return allowed;
}

void AuthMethodRestrictionList::allow(
    const QString& pathMask,
    AuthMethod::Value method)
{
    m_allowed[pathMask] = method;
}

void AuthMethodRestrictionList::deny(
    const QString& pathMask,
    AuthMethod::Value method)
{
    m_denied[pathMask] = method;
}

} // namespace nx_http
