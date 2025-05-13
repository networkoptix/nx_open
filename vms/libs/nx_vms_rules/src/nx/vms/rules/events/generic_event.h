// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/icon.h>

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API GenericEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "generic")
    FIELD(nx::Uuid, serverId, setServerId)
    FIELD(QString, caption, setCaption)
    FIELD(QString, description, setDescription)
    FIELD(QString, source, setSource)
    FIELD(UuidList, deviceIds, setDeviceIds)
    FIELD(bool, omitLogging, setOmitLogging)

public:
    GenericEvent() = default;
    GenericEvent(
        std::chrono::microseconds timestamp,
        State state,
        const QString& caption,
        const QString& description,
        const QString& source,
        nx::Uuid serverId,
        const UuidList& deviceIds);

    virtual QString aggregationKey() const override { return m_serverId.toSimpleString(); }
    virtual QVariantMap details(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const override;

    static const ItemDescriptor& manifest();

protected:
    virtual QString extendedCaption(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const override;

private:
    QStringList detailing(common::SystemContext* context) const;
    Icon icon() const;
};

} // namespace nx::vms::rules
