// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../data_macros.h"
#include "camera_event.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API CameraInputEvent: public CameraEvent
{
    Q_OBJECT
    using base_type = CameraEvent;
    Q_CLASSINFO("type", "nx.events.cameraInput")

    FIELD(QString, inputPortId, setInputPortId)

public:
    CameraInputEvent(
        std::chrono::microseconds timestamp,
        State state,
        nx::Uuid cameraId,
        const QString& inputPortId);

    CameraInputEvent() = default;

    virtual QString uniqueName() const override;
    virtual QString resourceKey() const override;
    virtual QString aggregationKey() const override;
    virtual QVariantMap details(common::SystemContext* context,
        const nx::vms::api::rules::PropertyMap& aggregatedInfo) const override;

    static ItemDescriptor manifest(common::SystemContext* context);

private:
    QString detailing() const;
    QString extendedCaption(common::SystemContext* context) const;
};

} // namespace nx::vms::rules
