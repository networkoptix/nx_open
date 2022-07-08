// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_conflict_event.h"

#include <nx/utils/range_adapters.h>

#include "../utils/event_details.h"
#include "../utils/string_helper.h"

namespace nx::vms::rules {

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

QString ServerConflictEvent::uniqueName() const
{
    return makeName(BasicEvent::uniqueName(), m_source.toSimpleString());
}

QString ServerConflictEvent::resourceKey() const
{
    return m_source.toSimpleString();
}

QVariantMap ServerConflictEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    if (!m_conflicts.sourceServer.isEmpty())
    {
        result[utils::kCaptionDetailName] =
            tr("Conflicting Server: %1").arg(m_conflicts.sourceServer);
    }

    utils::insertIfNotEmpty(result, utils::kDetailingDetailName, detailing());
    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);

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

QString ServerConflictEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = utils::StringHelper(context).resource({}, Qn::RI_WithUrl); //< TODO: add resource id to the event.
    return tr("Server \"%1\" Conflict").arg(resourceName);
}

const ItemDescriptor& ServerConflictEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.serverConflict",
        .displayName = tr("Server Conflict"),
        .description = "",
        .emailTemplatePath = ":/email_templates/mediaserver_conflict.mustache"
    };

    return kDescriptor;
}

} // namespace nx::vms::rules
