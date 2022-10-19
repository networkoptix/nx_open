// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "auth_restriction_list.h"

#include <nx/utils/string.h>
#include <nx/network/http/custom_headers.h>

namespace nx::network::http {

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

    NX_MUTEX_LOCKER lock(&m_mutex);

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

    // Allow auth without csrf for all requests proxied by server to the camera (see: VMS-16773).
    if (!request.getCookieValue(Qn::CAMERA_GUID_HEADER_NAME).empty())
        allowedMethods |= AuthMethod::allowWithoutCsrf;
    return allowedMethods;
}

void AuthMethodRestrictionList::allow(
    const Filter& filter,
    AuthMethod::Values authMethod)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
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
    NX_MUTEX_LOCKER lock(&m_mutex);
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

void AuthMethodRestrictionList::backupDeniedRulesForTests()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_deniedBackup = m_denied;
}

void AuthMethodRestrictionList::restoreDeniedRulesForTests()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_denied = std::move(m_deniedBackup);
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
    {
        pathRegexp = QRegularExpression(
            QRegularExpression::anchoredPattern(
                (QLatin1String("/*") + filter.path->c_str() + QLatin1String("/?"))));
    }
}

bool AuthMethodRestrictionList::Rule::matches(const Request& request) const
{
    if (filter.protocol && nx::utils::stricmp(
            *filter.protocol, request.requestLine.version.protocol) != 0)
    {
        return false;
    }

    if (filter.method && *filter.method != request.requestLine.method)
        return false;

    if (filter.path && !pathRegexp.match(request.requestLine.url.path()).hasMatch())
        return false;

    return true;
}

} // namespace nx::network::http
