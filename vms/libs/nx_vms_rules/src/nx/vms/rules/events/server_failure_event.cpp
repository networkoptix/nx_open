// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_failure_event.h"

#include "../group.h"
#include "../utils/event_details.h"
#include "../utils/string_helper.h"
#include "../utils/type.h"

namespace nx::vms::rules {

ServerFailureEvent::ServerFailureEvent(
    std::chrono::microseconds timestamp,
    QnUuid serverId,
    nx::vms::api::EventReason reason)
    :
    BasicEvent(timestamp),
    m_serverId(serverId),
    m_reason(reason)
{
}

QString ServerFailureEvent::resourceKey() const
{
    return m_serverId.toSimpleString();
}

QVariantMap ServerFailureEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    utils::insertIfNotEmpty(result, utils::kReasonDetailName, reason(context));
    utils::insertIfNotEmpty(result, utils::kDetailingDetailName, reason(context));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);
    utils::insertLevel(result, nx::vms::event::Level::critical);
    utils::insertIcon(result, nx::vms::rules::Icon::server);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::serverSettings);

    return result;
}

QString ServerFailureEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = utils::StringHelper(context).resource(serverId(), Qn::RI_WithUrl);
    return tr("Server \"%1\" Failure").arg(resourceName);
}

QString ServerFailureEvent::uniqueName() const
{
    return utils::makeName(
        BasicEvent::uniqueName(),
        QString::number(static_cast<int>(reason())));
}

QString ServerFailureEvent::reason(common::SystemContext* /*context*/) const
{
    using nx::vms::api::EventReason;

    switch (reason())
    {
        case EventReason::serverTerminated:
            return tr("Connection to server is lost.");

        case EventReason::serverStarted:
            return tr("Server stopped unexpectedly.");

        default:
            NX_ASSERT(0, "Unexpected reason");
            return {};
    }
}

const ItemDescriptor& ServerFailureEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<ServerFailureEvent>(),
        .groupId = kServerIssueEventGroup,
        .displayName = tr("Server Failure"),
        .permissions = {.globalPermission = GlobalPermission::powerUser},
        .emailTemplatePath = ":/email_templates/mediaserver_failure.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
