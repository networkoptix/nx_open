// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_event.h>
#include <nx/vms/api/types/resource_types.h>

namespace nx::vms::rules {

class NX_VMS_RULES_API CameraEvent: public BasicEvent
{
    Q_OBJECT

    Q_PROPERTY(QnUuid source READ source WRITE setSource)

public:
    QnUuid source() const;
    void setSource(QnUuid id);

    virtual QVariantMap details(common::SystemContext* context) const override;

protected:
    CameraEvent() = default;
    CameraEvent(std::chrono::microseconds timestamp, State state, QnUuid id);

private:
    QnUuid m_source;

    vms::api::ResourceStatus sourceStatus(common::SystemContext* context) const;
};

} // namespace nx::vms::rules
