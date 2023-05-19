// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <core/resource/media_server_resource.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API ServerResource: public QnMediaServerResource
{
    Q_OBJECT
    using base_type = QnMediaServerResource;

public:
    virtual void setStatus(
        nx::vms::api::ResourceStatus newStatus,
        Qn::StatusChangeReason reason = Qn::StatusChangeReason::Local) override;

    struct TimeZoneInfo
    {
        QString timezoneId;
        std::chrono::milliseconds utcOffset{0};

        bool operator==(const TimeZoneInfo& other) const = default;
    };

    /** Server local timezone. */
    TimeZoneInfo timeZoneInfo() const;
    void setTimeZoneInfo(TimeZoneInfo value);

protected:
    virtual void updateInternal(const QnResourcePtr& source, NotifierList& notifiers) override;

signals:
    void timeZoneInfoChanged();

private:
    TimeZoneInfo m_timeZoneInfo;
};

using ServerResourcePtr = QnSharedResourcePointer<ServerResource>;

} // namespace nx::vms::client::core
