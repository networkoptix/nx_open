// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_action.h"

#include "../action_builder_fields/content_type_field.h"
#include "../action_builder_fields/http_method_field.h"
#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/password_field.h"
#include "../action_builder_fields/text_field.h"
#include "../action_builder_fields/text_with_fields.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& HttpAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<HttpAction>(),
        .displayName = tr("Do HTTP(S) request"),
        .description = "",
        .fields = {
            utils::makeIntervalFieldDescriptor(tr("Interval of action")),
            makeFieldDescriptor<TextWithFields>("url", tr("HTTP(S) URL")),
            makeFieldDescriptor<TextWithFields>("content", tr("HTTP(S) Content")),
            makeFieldDescriptor<ContentTypeField>("contentType", tr("Content Type")),
            makeFieldDescriptor<ActionTextField>("login", tr("Login")),
            makeFieldDescriptor<PasswordField>("password", tr("Password")),
            makeFieldDescriptor<HttpMethodField>("method", tr("Request Method")),
            // TODO: #amalov Add auth type field.
        }
    };
    return kDescriptor;
}

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
