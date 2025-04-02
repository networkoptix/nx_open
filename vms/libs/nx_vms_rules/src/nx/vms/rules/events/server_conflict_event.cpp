// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_conflict_event.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_context.h>

#include "../group.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

ServerConflictEvent::ServerConflictEvent(
    std::chrono::microseconds timestamp,
    nx::Uuid serverId,
    const CameraConflictList& conflicts)
    :
    BasicEvent(timestamp),
    m_serverId(serverId),
    m_conflicts(conflicts)
{
}

QVariantMap ServerConflictEvent::details(
    common::SystemContext* context,
    const nx::vms::api::rules::PropertyMap& aggregatedInfo,
    Qn::ResourceInfoLevel detailLevel) const
{
    auto result = BasicEvent::details(context, aggregatedInfo, detailLevel);
    fillAggregationDetailsForServer(result, context, serverId(), detailLevel);

    utils::insertLevel(result, nx::vms::event::Level::important);
    utils::insertIcon(result, nx::vms::rules::Icon::server);

    const bool isServerIdConflict = m_conflicts.camerasByServer.empty();

    const QString reason = isServerIdConflict
        ? tr("Discovered a server with the same ID in the same local network")
        : tr("Servers in the same local network have conflict on the following devices");

    QStringList detailing{{reason + ":"}};
    QStringList htmlDetails{{reason}};

    const QString serverRow = tr("Server: %1");
    const QString textRow("- %1");

    if (isServerIdConflict)
    {
        detailing.push_back(serverRow.arg(m_conflicts.sourceServer));
        htmlDetails.push_back(m_conflicts.sourceServer);
    }
    else
    {
        QStringList htmlRows;

        for (const auto& [server, cameras]: m_conflicts.camerasByServer.asKeyValueRange())
        {
            detailing.push_back(serverRow.arg(server));
            htmlRows.push_back(common::html::bold(serverRow.arg(server)));

            for (const QString& id: cameras)
            {
                auto device = context->resourcePool()->getCameraByPhysicalId(id);
                const QString deviceName = device
                    ? Strings::resource(device, Qn::RI_WithUrl)
                    : id;
                detailing.push_back(textRow.arg(deviceName));
                htmlRows.push_back(deviceName);
            }
        }

        htmlDetails.push_back(htmlRows.join(common::html::kLineBreak));
    }

    result[utils::kDetailingDetailName] = detailing;
    result[utils::kHtmlDetailsName] = htmlDetails;

    return result;
}

QString ServerConflictEvent::extendedCaption(common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    const auto resourceName = Strings::resource(context, serverId(), detailLevel);
    return tr("%1 Conflict", "Server name will be substituted").arg(resourceName);
}

const ItemDescriptor& ServerConflictEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<ServerConflictEvent>(),
        .groupId = kServerIssueEventGroup,
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Server Conflict")),
        .description = "Triggered when multiple servers on the same network access "
            "the same devices.",
        .resources = {{utils::kServerIdFieldName, {ResourceType::server}}},
        .readPermissions = GlobalPermission::powerUser
    };

    return kDescriptor;
}

} // namespace nx::vms::rules
