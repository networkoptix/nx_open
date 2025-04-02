// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API CameraInputEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "cameraInput")
    FIELD(nx::Uuid, deviceId, setDeviceId)
    FIELD(QString, inputPortId, setInputPortId)

public:
    CameraInputEvent(
        std::chrono::microseconds timestamp,
        State state,
        nx::Uuid deviceId,
        const QString& inputPortId);

    CameraInputEvent() = default;

    virtual QString aggregationKey() const override { return m_deviceId.toSimpleString(); }
    virtual QString sequenceKey() const override;
    virtual QVariantMap details(
        common::SystemContext* context,
        const nx::vms::api::rules::PropertyMap& aggregatedInfo,
        Qn::ResourceInfoLevel detailLevel) const override;

    static ItemDescriptor manifest(common::SystemContext* context);

protected:
    virtual QString extendedCaption(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const override;

private:
    QString detailing() const;
};

} // namespace nx::vms::rules
