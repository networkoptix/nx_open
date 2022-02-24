// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_action.h"

namespace nx::vms::rules {

QString HttpAction::url() const
{
    return m_url;
}

void HttpAction::setUrl(const QString& url)
{
    m_url = url;
}

QString HttpAction::content() const
{
    return m_content;
}

void HttpAction::setContent(const QString& content)
{
    m_content = content;
}

HttpAction::ContentType HttpAction::contentType() const
{
    return m_contentType;
}

void HttpAction::setContentType(ContentType contentType)
{
    m_contentType = contentType;
}

QString HttpAction::login() const
{
    return m_login;
}

void HttpAction::setLogin(const QString& login)
{
    m_login = login;
}

QString HttpAction::password() const
{
    return m_password;
}

void HttpAction::setPassword(const QString& password)
{
    m_password = password;
}

} // namespace nx::vms::rules