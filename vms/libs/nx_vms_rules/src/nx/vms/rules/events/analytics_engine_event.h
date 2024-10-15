// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "described_event.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API AnalyticsEngineEvent: public DescribedEvent
{
    Q_OBJECT

    Q_PROPERTY(nx::Uuid cameraId READ cameraId WRITE setCameraId)
    Q_PROPERTY(nx::Uuid engineId READ engineId WRITE setEngineId)

public:
    AnalyticsEngineEvent() = default;
    AnalyticsEngineEvent(
        std::chrono::microseconds timestamp,
        const QString& caption,
        const QString& description,
        nx::Uuid cameraId,
        nx::Uuid engineId);

    nx::Uuid cameraId() const;
    void setCameraId(nx::Uuid cameraId);
    nx::Uuid engineId() const;
    void setEngineId(nx::Uuid engineId);

    virtual QString resourceKey() const override;
    virtual QVariantMap details(common::SystemContext* context,
        const nx::vms::api::rules::PropertyMap& aggregatedInfo) const override;

protected:
    virtual QStringList detailing() const;

private:
    nx::Uuid m_cameraId;
    nx::Uuid m_engineId;
};

} // namespace nx::vms::rules
