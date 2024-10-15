// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_conflict_event.h"

#include <nx/utils/range_adapters.h>

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

QString ServerConflictEvent::resourceKey() const
{
    return m_serverId.toSimpleString();
}

QVariantMap ServerConflictEvent::details(
    common::SystemContext* context, const nx::vms::api::rules::PropertyMap& aggregatedInfo) const
{
    auto result = BasicEvent::details(context, aggregatedInfo);

    result.insert(utils::kExtendedCaptionDetailName, extendedCaption(context));
    result.insert(utils::kDetailingDetailName, detailing());
    result.insert(utils::kDescriptionDetailName, detailing());
    utils::insertLevel(result, nx::vms::event::Level::important);
    utils::insertIcon(result, nx::vms::rules::Icon::server);

    return result;
}

QStringList ServerConflictEvent::detailing() const
{
    QStringList result;

    int n = 0;
    for (const auto& [server, cameras]: nx::utils::constKeyValueRange(m_conflicts.camerasByServer))
    {
        //: Conflicting Server #5: 10.0.2.1
        result << tr("Conflicting Server #%1: %2").arg(++n).arg(server);
        int m = 0;
        //: MAC #2: D0-50-99-38-1E-12
        for (const QString &camera: cameras)
            result << tr("MAC #%1: %2").arg(++m).arg(camera);
    }

    if (result.empty())
        result << tr("Conflicting Server: %1").arg(m_conflicts.sourceServer);

    return result;
}

QString ServerConflictEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = Strings::resource(context, serverId(), Qn::RI_WithUrl);
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
        .readPermissions = GlobalPermission::powerUser,
        .emailTemplateName = "timestamp_and_details.mustache"
    };

    return kDescriptor;
}

} // namespace nx::vms::rules
