// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_auth_field.h"

namespace nx::vms::rules {

nx::network::http::AuthType HttpAuthField::authType() const
{
    return m_auth.authType;
}

void HttpAuthField::setAuthType(const nx::network::http::AuthType& authType)
{
    if (m_auth.authType != authType)
    {
        m_auth.authType = authType;
        emit authTypeChanged();
    }
}

std::string HttpAuthField::login() const
{
    return m_auth.credentials.user;
}

void HttpAuthField::setLogin(const std::string& login)
{
    if (m_auth.credentials.user != login)
    {
        m_auth.credentials.user = login;
        emit loginChanged();
    }
}

std::string HttpAuthField::password() const
{
    return m_auth.credentials.password ? m_auth.credentials.password.value() : "";
}

void HttpAuthField::setPassword(const std::string& password)
{
    if (m_auth.credentials.password != password)
    {
        m_auth.credentials.password = password.empty() ? std::optional<std::string>() : password;
        emit passwordChanged();

        if (!password.empty())
            setToken({});
    }
}

std::string HttpAuthField::token() const
{
    return m_auth.credentials.token ? m_auth.credentials.token.value() : "";
}

void HttpAuthField::setToken(const std::string& token)
{
    if (m_auth.credentials.token != token)
    {
        m_auth.credentials.token = token.empty() ? std::optional<std::string>() : token;
        emit tokenChanged();

        if (!token.empty())
            setPassword({});
    }
}

AuthenticationInfo HttpAuthField::auth() const
{
    return m_auth;
}

void HttpAuthField::setAuth(const AuthenticationInfo& auth)
{
    if (m_auth != auth)
    {
        m_auth = auth;
        emit authChanged();
    }
}

QVariant HttpAuthField::build(const AggregatedEventPtr& /*eventAggregator*/) const
{
    return QVariant::fromValue(m_auth);
}

} // namespace nx::vms::rules
