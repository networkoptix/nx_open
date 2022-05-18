// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_conflict_event.h"

#include <nx/utils/range_adapters.h>

#include "../utils/event_details.h"

namespace nx::vms::rules {

const ItemDescriptor& ServerConflictEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.serverConflict",
        .displayName = tr("Server Conflict"),
        .description = "",
    };

    return kDescriptor;
}

ServerConflictEvent::ServerConflictEvent(
    std::chrono::microseconds timestamp,
    QnUuid serverId,
    const CameraConflictList& conflicts)
    :
    BasicEvent(timestamp),
    m_source(serverId),
    m_conflicts(conflicts)
{
}

QMap<QString, QString> ServerConflictEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    if (!m_conflicts.sourceServer.isEmpty())
    {
        result[utils::kCaptionDetailName] =
            tr("Conflicting Server: %1").arg(m_conflicts.sourceServer);
    }

    utils::insertIfNotEmpty(result, utils::kDetailingDetailName, detailing());

    return result;
}

QString ServerConflictEvent::detailing() const
{
    // See original formatting in StringsHelper::eventDetails()
    if (m_conflicts.camerasByServer.isEmpty())
        return {};

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

    //TODO: #mmalofeev Choose type for tooltip/detailing field.
    return result.join("\n");
}

} // namespace nx::vms::rules
