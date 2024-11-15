// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API LicenseIssueEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "licenseIssue")

    FIELD(nx::Uuid, serverId, setServerId)
    FIELD(UuidList, deviceIds, deviceIds)

public:
    LicenseIssueEvent() = default;
    LicenseIssueEvent(
        std::chrono::microseconds timestamp,
        nx::Uuid serverId,
        const UuidSet& disabledCameras);

    virtual QString resourceKey() const override;
    virtual QString aggregationSubKey() const override;
    static const ItemDescriptor& manifest();

protected:
    virtual QVariantMap details(common::SystemContext* context,
        const nx::vms::api::rules::PropertyMap& aggregatedInfo) const override;

private:
    QString extendedCaption(common::SystemContext* context) const;
    QString reason(nx::vms::common::SystemContext* context) const;
    QStringList detailing(nx::vms::common::SystemContext* context) const;
};

} // namespace nx::vms::rules
