// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/types/event_rule_types.h>

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API IntegrationDiagnosticEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "integrationDiagnostic")
    FIELD(nx::Uuid, deviceId, setDeviceId)
    FIELD(nx::Uuid, engineId, setEngineId)
    FIELD(QString, caption, setCaption)
    FIELD(QString, description, setDescription)
    FIELD(nx::vms::api::EventLevel, level, setLevel)

public:
    IntegrationDiagnosticEvent() = default;
    IntegrationDiagnosticEvent(
        std::chrono::microseconds timestamp,
        const QString& caption,
        const QString& description,
        nx::Uuid deviceId,
        nx::Uuid engineId,
        nx::vms::api::EventLevel level);

    /** Aggregation key differs based on the device presence. */
    virtual QString aggregationKey() const override;
    virtual QVariantMap details(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const override;

    static const ItemDescriptor& manifest();

protected:
    virtual QString extendedCaption(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const override;

private:
    /** Diagnostic events can be produced without a device, with engine only. */
    nx::Uuid aggregationResourceId() const;
    QStringList detailing() const;
};

} // namespace nx::vms::rules
