// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_event.h>

#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API CameraInputEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.cameraInput")

    FIELD(QnUuid, cameraId, setCameraId)
    FIELD(QString, inputPortId, setInputPortId)

public:
    CameraInputEvent(std::chrono::microseconds timestamp, const QnUuid& cameraId):
        BasicEvent(timestamp),
        m_cameraId(cameraId)
    {
    }

    virtual QString uniqueName() const override;

    virtual QMap<QString, QString> details(common::SystemContext* context) const override;

    static FilterManifest filterManifest();

private:
    QString detailing() const;
};

} // namespace nx::vms::rules
