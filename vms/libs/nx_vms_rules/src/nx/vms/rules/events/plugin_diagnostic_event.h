// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_event.h>

#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API PluginDiagnosticEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.pluginDiagnostic")

    FIELD(QnUuid, cameraId, setCameraId)
    FIELD(QnUuid, engineId, setEngineId)
    FIELD(QString, caption, setCaption)
    FIELD(QString, description, setDescription)
    // TODO: Introduce level type.
    FIELD(int, level, setLevel)

public:
    PluginDiagnosticEvent() = default;

    PluginDiagnosticEvent(
        QnUuid cameraId,
        QnUuid engineId,
        const QString &caption,
        const QString &description,
        int level)
        :
        m_cameraId(cameraId),
        m_engineId(engineId),
        m_caption(caption),
        m_description(description),
        m_level(level)

    {
    }

    static FilterManifest filterManifest();
};

} // namespace nx::vms::rules
