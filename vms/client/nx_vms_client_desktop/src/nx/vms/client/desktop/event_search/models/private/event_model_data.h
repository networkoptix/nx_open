// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/vms/api/rules/event_log.h>
#include <nx/vms/rules/rules_fwd.h>

namespace nx::vms::client::core { class SystemContext; }

namespace nx::vms::client::desktop {

class SystemContext;

class EventModelData
{
public:
    EventModelData() = delete;

    EventModelData(const nx::vms::api::rules::EventLogRecord& record);

    const nx::vms::api::rules::EventLogRecord& record() const;

    nx::vms::rules::EventPtr event(core::SystemContext* context) const;
    const QVariantMap& details(SystemContext* context) const;
    QString eventType() const;

protected:
    nx::vms::api::rules::EventLogRecord m_record;

private:
    mutable nx::vms::rules::EventPtr m_event;
    mutable QVariantMap m_details;
};

class EventLogModelData: public EventModelData
{
public:
    using EventModelData::EventModelData;

    /** Special comparison string for event dialog column sorting. */
    void setCompareString(const QString& str);
    const QString& compareString() const;

    nx::vms::rules::ActionPtr action(SystemContext* context) const;
    const QVariantMap actionDetails(SystemContext* context) const;
    QString actionType() const;

private:
    mutable nx::vms::rules::ActionPtr m_action;
    mutable QVariantMap m_actionDetails;
    QString m_compareString;
};

using EventModelDataList = std::vector<EventModelData>;
using EventLogModelDataList = std::vector<EventLogModelData>;

} // namespace nx::vms::client::desktop
