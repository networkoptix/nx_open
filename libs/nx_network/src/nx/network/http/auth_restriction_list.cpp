// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "auth_restriction_list.h"

#include <nx/utils/string.h>
#include <nx/network/http/custom_headers.h>

namespace nx::network::http {

AuthMethodRestrictionList::AuthMethodRestrictionList(AuthMethods allowedByDefault):
    m_allowedByDefault(allowedByDefault)
{
}

AuthMethods AuthMethodRestrictionList::getAllowedAuthMethods(
    const nx::network::http::Request& request) const
{
    AuthMethods allowedMethods = m_allowedByDefault;

    NX_READ_LOCKER locker(&m_mutex);

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
    AuthMethods authMethod)
{
    NX_WRITE_LOCKER locker(&m_mutex);
    m_allowed.emplace_back(filter, authMethod);
}

void AuthMethodRestrictionList::allow(
    const std::string& pathMask,
    AuthMethods method)
{
    Filter filter;
    filter.path = pathMask;
    allow(filter, method);
}

void AuthMethodRestrictionList::deny(
    const Filter& filter,
    AuthMethods authMethod)
{
    NX_WRITE_LOCKER locker(&m_mutex);
    m_denied.emplace_back(filter, authMethod);
}

void AuthMethodRestrictionList::deny(
    const std::string& pathMask,
    AuthMethods method)
{
    Filter filter;
    filter.path = pathMask;
    deny(filter, method);
}

void AuthMethodRestrictionList::backupDeniedRulesForTests()
{
    NX_READ_LOCKER lock(&m_mutex);
    m_deniedBackup = m_denied;
}

void AuthMethodRestrictionList::restoreDeniedRulesForTests()
{
    NX_WRITE_LOCKER lock(&m_mutex);
    m_denied = std::move(m_deniedBackup);
}

//-------------------------------------------------------------------------------------------------

AuthMethodRestrictionList::Rule::Rule(const Filter& filter, AuthMethods methods):
    filter(filter), methods(methods)
{
    if (filter.path)
        pathRegexp = std::regex(filter.path->c_str());
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

    using namespace std;
    if (filter.path && !regex_match(request.requestLine.url.path().toUtf8().data(), pathRegexp))
        return false;

    return true;
}

} // namespace nx::network::http
