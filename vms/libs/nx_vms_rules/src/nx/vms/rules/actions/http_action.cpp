// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_action.h"

#include "../action_builder_fields/content_type_field.h"
#include "../action_builder_fields/http_auth_field.h"
#include "../action_builder_fields/http_auth_type_field.h"
#include "../action_builder_fields/http_method_field.h"
#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/text_field.h"
#include "../action_builder_fields/text_with_fields.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

using namespace std::chrono_literals;

namespace nx::vms::rules {

const ItemDescriptor& HttpAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<HttpAction>(),
        .displayName = tr("HTTP(S) Request"),
        .description = "",
        .fields = {
            makeFieldDescriptor<TextWithFields>("url", tr("URL")),
            makeFieldDescriptor<TextWithFields>("content", tr("Content")),
            makeFieldDescriptor<ContentTypeField>("contentType", tr("Content type")),
            makeFieldDescriptor<HttpMethodField>("method", tr("Method")),
            makeFieldDescriptor<HttpAuthField>("auth", tr("HTTP authentication")),
            utils::makeTimeFieldDescriptor<OptionalTimeField>(
                utils::kIntervalFieldName,
                tr("Interval of Action"),
                {},
                {.initialValue = 0s, .defaultValue = 1s, .maximumValue = std::chrono::days(24855), .minimumValue = 1s})
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

std::string HttpAction::login() const
{
    return m_auth.credentials.user;
}

std::string HttpAction::password() const
{
    return m_auth.credentials.password ? m_auth.credentials.password.value() : "";
}

AuthenticationInfo HttpAction::auth() const
{
    return m_auth;
}

void HttpAction::setAuth(const AuthenticationInfo& auth)
{
    m_auth = auth;
}

nx::network::http::AuthType HttpAction::authType() const
{
    return m_auth.authType;
}

std::string HttpAction::token() const
{
    return m_auth.credentials.token ? m_auth.credentials.token.value() : "";
}

QVariantMap HttpAction::details(common::SystemContext* context) const
{
    auto result = BasicAction::details(context);
    result[utils::kDestinationDetailName] = QUrl(url()).toString(QUrl::RemoveUserInfo);

    return result;
}

} // namespace nx::vms::rules
