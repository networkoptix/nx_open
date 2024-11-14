// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_action.h"

#include "../action_builder_fields/content_type_field.h"
#include "../action_builder_fields/http_auth_field.h"
#include "../action_builder_fields/http_method_field.h"
#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/text_with_fields.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

using namespace std::chrono_literals;

namespace nx::vms::rules {

const ItemDescriptor& HttpAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<HttpAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("HTTP(S) Request")),
        .description = "Send HTTP(S) request.",
        .executionTargets = ExecutionTarget::servers,
        .targetServers = TargetServers::currentServer,
        .fields = {
            makeFieldDescriptor<TextWithFields>("url",
                NX_DYNAMIC_TRANSLATABLE(tr("URL")),
                {},
                TextWithFieldsFieldProperties{
                    .base = FieldProperties{.optional = false},
                    .validationPolicy = kUrlValidationPolicy}.toVariantMap()),
            makeFieldDescriptor<HttpMethodField>("method",
                NX_DYNAMIC_TRANSLATABLE(tr("Method")),
                {},
                {{"value", QString(network::http::Method::get.data())}}),
            makeFieldDescriptor<TextWithFields>(utils::kContentFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Content")),
                {},
                TextWithFieldsFieldProperties{
                    .base = FieldProperties{.optional = false},
                    // Json strings are often used in this field, but the substitution syntax - `{}`
                    // conflicts with the JSON syntax. To avoid highlighting all the text as
                    // invalid, we decided to disable highlighting for this field altogether.
                    .highlightErrors = false}.toVariantMap()),
            makeFieldDescriptor<ContentTypeField>("contentType",
                NX_DYNAMIC_TRANSLATABLE(tr("Content type")),
                "If not set, it will be calculated based on the content value."),
            makeFieldDescriptor<HttpAuthField>("auth",
                NX_DYNAMIC_TRANSLATABLE(tr("HTTP authentication")),
                "HTTP request authentication",
                FieldProperties{.optional = false}.toVariantMap()),
            utils::makeIntervalFieldDescriptor(
                Strings::intervalOfAction(),
                {},
                TimeFieldProperties{.maximumValue = std::chrono::days(24855)}.toVariantMap())
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
