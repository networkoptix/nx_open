// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QTimeZone>

#include <core/resource/media_server_resource.h>
#include <nx/vms/api/data/server_timezone_information.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API ServerResource: public QnMediaServerResource
{
    Q_OBJECT
    using base_type = QnMediaServerResource;

public:
    ServerResource();

    virtual void setStatus(
        nx::vms::api::ResourceStatus newStatus,
        Qn::StatusChangeReason reason = Qn::StatusChangeReason::Local) override;

    /**
     * Native server timezone (if available), or timezone from the UTC offset as a fallback.
     */
    QTimeZone timeZone() const;

    /** Server local timezone information. */
    nx::vms::api::ServerTimeZoneInformation timeZoneInfo() const;

    /**
     * Set server timezone info externally. Workaround to maintain mobile client connect to older
     * servers. Saves data to server if possible.
     */
    void overrideTimeZoneInfo(const nx::vms::api::ServerTimeZoneInformation& value);

protected:
    virtual void atPropertyChanged(const QnResourcePtr& self, const QString& key) override;

private:
    nx::vms::api::ServerTimeZoneInformation calculateTimeZoneInfo() const;
    QTimeZone calculateTimeZone() const;

signals:
    void timeZoneChanged();

private:
    nx::utils::CachedValue<nx::vms::api::ServerTimeZoneInformation> m_timeZoneInfo;
    nx::utils::CachedValue<QTimeZone> m_timeZone;
};

using ServerResourcePtr = QnSharedResourcePointer<ServerResource>;

} // namespace nx::vms::client::core
