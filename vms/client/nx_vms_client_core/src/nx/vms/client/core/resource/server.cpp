// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server.h"

#include <chrono>

#include <core/resource/camera_resource.h>
#include <core/resource/resource_property_key.h>
#include <core/resource_management/resource_pool.h>
#include <nx/reflect/json.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/time/timezone.h>

namespace nx::vms::client::core {

ServerResource::ServerResource():
    QnMediaServerResource(),
    m_timeZoneInfo([this]() { return calculateTimeZoneInfo(); }),
    m_timeZone([this]() { return calculateTimeZone(); })
{
}

void ServerResource::setStatus(
    nx::vms::api::ResourceStatus newStatus,
    Qn::StatusChangeReason reason)
{
    base_type::setStatus(newStatus, reason);
    if (auto resourcePool = this->resourcePool())
    {
        auto childCameras = resourcePool->getAllCameras(getId());
        for (const auto& camera: childCameras)
        {
            NX_VERBOSE(this, "%1 Emit statusChanged signal for resource %2", __func__, camera);
            emit camera->statusChanged(camera, Qn::StatusChangeReason::Local);
        }
    }
}

QTimeZone ServerResource::timeZone() const
{
    return m_timeZone.get();
}

nx::vms::api::ServerTimeZoneInformation ServerResource::timeZoneInfo() const
{
    return m_timeZoneInfo.get();
}

void ServerResource::overrideTimeZoneInfo(const nx::vms::api::ServerTimeZoneInformation& value)
{
    setProperty(ResourcePropertyKey::Server::kTimeZoneInformation,
        QString::fromStdString(nx::reflect::json::serialize(value)));

    // This call will succeed only for 5.1 and older servers and only if logged in as admin. This
    // workaround will be useful for the mobile client when connecting to older servers.
    savePropertiesAsync();
}

void ServerResource::atPropertyChanged(const QnResourcePtr& self, const QString& key)
{
    base_type::atPropertyChanged(self, key);
    if (key == ResourcePropertyKey::Server::kTimeZoneInformation)
    {
        m_timeZoneInfo.reset();
        m_timeZone.reset();
        emit timeZoneChanged();
    }
}

nx::vms::api::ServerTimeZoneInformation ServerResource::calculateTimeZoneInfo() const
{
    const QString value = getProperty(ResourcePropertyKey::Server::kTimeZoneInformation);

    nx::vms::api::ServerTimeZoneInformation result;
    if (!value.isEmpty())
    {
        nx::reflect::json::deserialize<nx::vms::api::ServerTimeZoneInformation>(
            value.toStdString(),
            &result);
    }
    return result;
}

QTimeZone ServerResource::calculateTimeZone() const
{
    return nx::vms::time::timeZone(timeZoneInfo().timeZoneId, timeZoneInfo().timeZoneOffsetMs);
}

} // namespace nx::vms::client::core
