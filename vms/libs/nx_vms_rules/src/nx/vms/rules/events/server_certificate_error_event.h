// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API ServerCertificateErrorEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "serverCertificateError")
    FIELD(nx::Uuid, serverId, setServerId)

public:
    ServerCertificateErrorEvent() = default;
    ServerCertificateErrorEvent(std::chrono::microseconds timestamp, nx::Uuid serverId);

    virtual QString aggregationKey() const override { return m_serverId.toSimpleString(); }
    virtual QVariantMap details(
        common::SystemContext* context,
        const nx::vms::api::rules::PropertyMap& aggregatedInfo,
        Qn::ResourceInfoLevel detailLevel) const override;

    static const ItemDescriptor& manifest();

protected:
    virtual QString extendedCaption(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const override;
};

} // namespace nx::vms::rules
