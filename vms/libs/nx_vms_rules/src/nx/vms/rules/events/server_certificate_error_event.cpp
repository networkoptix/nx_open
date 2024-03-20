// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_certificate_error_event.h"

#include "../group.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/string_helper.h"
#include "../utils/type.h"

namespace nx::vms::rules {

ServerCertificateErrorEvent::ServerCertificateErrorEvent(
    std::chrono::microseconds timestamp,
    nx::Uuid serverId)
    :
    base_type(timestamp),
    m_serverId(serverId)
{
}

QString ServerCertificateErrorEvent::resourceKey() const
{
    return m_serverId.toSimpleString();
}

QVariantMap ServerCertificateErrorEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);
    utils::insertLevel(result, nx::vms::event::Level::critical);
    utils::insertIcon(result, nx::vms::rules::Icon::server);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::serverSettings);

    return result;
}

QString ServerCertificateErrorEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = utils::StringHelper(context).resource(m_serverId, Qn::RI_WithUrl);
    return tr("Server \"%1\" certificate error").arg(resourceName);
}

const ItemDescriptor& ServerCertificateErrorEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<ServerCertificateErrorEvent>(),
        .groupId = kServerIssueEventGroup,
        .displayName = tr("Server Certificate Error"),
        .resources = {{utils::kServerIdFieldName, {ResourceType::Server}}},
        .readPermissions = GlobalPermission::powerUser,
        .emailTemplatePath = ":/email_templates/server_certificate_error.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
