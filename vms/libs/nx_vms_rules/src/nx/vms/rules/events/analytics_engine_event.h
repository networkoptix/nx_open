// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "described_event.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API AnalyticsEngineEvent: public DescribedEvent
{
    Q_OBJECT

    Q_PROPERTY(QnUuid cameraId READ cameraId WRITE setCameraId)
    Q_PROPERTY(QnUuid engineId READ engineId WRITE setEngineId)

public:
    AnalyticsEngineEvent() = default;
    AnalyticsEngineEvent(
        std::chrono::microseconds timestamp,
        const QString& caption,
        const QString& description,
        QnUuid cameraId,
        QnUuid engineId);

    QnUuid cameraId() const;
    void setCameraId(QnUuid cameraId);
    QnUuid engineId() const;
    void setEngineId(QnUuid engineId);

    virtual QString uniqueName() const override;
    virtual QString resourceKey() const override;
    virtual QVariantMap details(common::SystemContext* context) const override;

private:
    QnUuid m_cameraId;
    QnUuid m_engineId;
};

} // namespace nx::vms::rules
