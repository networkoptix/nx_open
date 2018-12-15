#include "auth_restriction_list.h"

#include <nx/utils/string.h>

namespace nx::network::http {

const AuthMethod::Values AuthMethodRestrictionList::kDefaults =
    AuthMethod::sessionKey | AuthMethod::cookie | AuthMethod::http |
    AuthMethod::videowallHeader | AuthMethod::urlQueryDigest;

AuthMethodRestrictionList::AuthMethodRestrictionList(
    AuthMethod::Values allowedByDefault)
    :
    m_allowedByDefault(allowedByDefault)
{
}

AuthMethod::Values AuthMethodRestrictionList::getAllowedAuthMethods(
    const nx::network::http::Request& request) const
{
    AuthMethod::Values allowedMethods = m_allowedByDefault;

    QnMutexLocker lock(&m_mutex);

    for (const auto& rule: m_allowed)
    {
        if (rule.matches(request))
            allowedMethods |= rule.methods;
    }

    for (const auto& rule: m_denied)
    {
        if (rule.matches(request))
            allowedMethods &= ~rule.methods;
    }

    return allowedMethods;
}

void AuthMethodRestrictionList::allow(
    const Filter& filter,
    AuthMethod::Values authMethod)
{
    QnMutexLocker lock(&m_mutex);
    m_allowed.emplace_back(filter, authMethod);
}

void AuthMethodRestrictionList::allow(
    const std::string& pathMask,
    AuthMethod::Values method)
{
    Filter filter;
    filter.path = pathMask;
    allow(filter, method);
}

void AuthMethodRestrictionList::deny(
    const Filter& filter,
    AuthMethod::Values authMethod)
{
    QnMutexLocker lock(&m_mutex);
    m_denied.emplace_back(filter, authMethod);
}

void AuthMethodRestrictionList::deny(
    const std::string& pathMask,
    AuthMethod::Values method)
{
    Filter filter;
    filter.path = pathMask;
    deny(filter, method);
}

//-------------------------------------------------------------------------------------------------

AuthMethodRestrictionList::Rule::Rule(
    const Filter& filter,
    AuthMethod::Values methods)
    :
    filter(filter),
    methods(methods)
{
    if (filter.path)
        pathRegexp = QRegExp(QLatin1String("/*") + filter.path->c_str() + QLatin1String("/?"));
}

bool AuthMethodRestrictionList::Rule::matches(const Request& request) const
{
    if (filter.protocol && nx::utils::stricmp(
            *filter.protocol, request.requestLine.version.protocol.toStdString()) != 0)
    {
        return false;
    }

    if (filter.method && nx::utils::stricmp(
            *filter.method, request.requestLine.method.toStdString()) != 0)
    {
        return false;
    }

    if (filter.path && !pathRegexp.exactMatch(request.requestLine.url.path()))
        return false;

    return true;
}

} // namespace nx::network::http
