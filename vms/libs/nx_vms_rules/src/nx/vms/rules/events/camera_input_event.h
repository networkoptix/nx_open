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
    static const ItemDescriptor& manifest();

    CameraInputEvent(
        std::chrono::microseconds timestamp,
        State state,
        QnUuid cameraId,
        const QString& inputPortId);

    CameraInputEvent() = default;

    virtual QString uniqueName() const override;

    virtual QMap<QString, QString> details(common::SystemContext* context) const override;

private:
    QString detailing() const;
};

} // namespace nx::vms::rules
