// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "site_http_action.h"

#include "../action_builder_fields/http_method_field.h"
#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/text_with_fields.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

using namespace std::chrono_literals;

namespace nx::vms::rules {

const ItemDescriptor& SiteHttpAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<SiteHttpAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Site HTTP(S) Request")),
        .description = "Send HTTP(S) request to the site.",
        .executionTargets = ExecutionTarget::servers,
        .targetServers = TargetServers::currentServer,
        .fields = {
            makeFieldDescriptor<TextWithFields>(utils::kEndpointFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Endpoint")),
                "Endpoint supported by the given site.",
                TextWithFieldsFieldProperties{
                    .base = FieldProperties{.optional = false},
                    .validationPolicy = kSiteEndpointValidationPolicy}.toVariantMap()),
            makeFieldDescriptor<HttpMethodField>(utils::kMethodFieldName,
                Strings::method(),
                {"If not set, it will be calculated automatically."}),
            makeFieldDescriptor<TextWithFields>(utils::kContentFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Content")),
                {},
                TextWithFieldsFieldProperties{
                    .base = FieldProperties{.optional = false},
                    // Json strings are often used in this field, but the substitution syntax - `{}`
                    // conflicts with the JSON syntax. To avoid highlighting all the text as
                    // invalid, we decided to disable highlighting for this field altogether.
                    .highlightErrors = false}.toVariantMap()),
            utils::makeIntervalFieldDescriptor(
                Strings::intervalOfAction(),
                {},
                TimeFieldProperties{
                    .value = std::chrono::minutes(1),
                    .maximumValue = std::chrono::days(24855)}.toVariantMap())
        }
    };
    return kDescriptor;
}

QVariantMap SiteHttpAction::details(common::SystemContext* context) const
{
    auto result = BasicAction::details(context);
    result[utils::kDestinationDetailName] = m_endpoint;

    return result;
}

} // namespace nx::vms::rules
