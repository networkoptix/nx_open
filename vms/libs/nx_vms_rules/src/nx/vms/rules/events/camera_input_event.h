// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API CameraInputEvent: public BasicEvent
{
    Q_OBJECT
    using base_type = BasicEvent;
    Q_CLASSINFO("type", "cameraInput")

    FIELD(QString, inputPortId, setInputPortId)
    FIELD(nx::Uuid, cameraId, setCameraId)

public:
    CameraInputEvent(
        std::chrono::microseconds timestamp,
        State state,
        nx::Uuid cameraId,
        const QString& inputPortId);

    CameraInputEvent() = default;

    virtual QString resourceKey() const override;
    virtual QString aggregationKey() const override;
    virtual QString aggregationSubKey() const override;
    virtual QVariantMap details(common::SystemContext* context,
        const nx::vms::api::rules::PropertyMap& aggregatedInfo) const override;

    static ItemDescriptor manifest(common::SystemContext* context);

private:
    QString detailing() const;
    QString extendedCaption(common::SystemContext* context) const;
};

} // namespace nx::vms::rules
