// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ptz_availability_watcher.h"

#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/ptz/client_ptz_controller_pool.h>
#include <nx/vms/client/core/resource/camera.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/watchers/user_watcher.h>

namespace {

#if defined(NO_MOBILE_PTZ_SUPPORT)
    static constexpr bool kSupportMobilePtz = false;
#else
    static constexpr bool kSupportMobilePtz = true;
#endif

} // namespace

namespace nx {
namespace client {
namespace mobile {

PtzAvailabilityWatcher::PtzAvailabilityWatcher(
    const nx::vms::client::core::CameraPtr& camera,
    QObject* parent)
    :
    base_type(parent),
    m_camera(camera)
{
    if (!NX_ASSERT(camera))
        return;

    auto systemContext = nx::vms::client::core::SystemContext::fromResource(camera);
    if (!NX_ASSERT(systemContext))
        return;

    const auto userWatcher = systemContext->userWatcher();
    const auto notifier = systemContext->resourceAccessManager()->createNotifier(this);

    connect(userWatcher, &nx::vms::client::core::UserWatcher::userChanged, this,
        [this, notifier](const QnUserResourcePtr& user)
        {
            notifier->setSubjectId(user ? user->getId() : nx::Uuid{});
            updateAvailability();
        });

    connect(notifier, &QnResourceAccessManager::Notifier::resourceAccessChanged,
        this, &PtzAvailabilityWatcher::updateAvailability);

    connect(systemContext->resourceAccessManager(), &QnResourceAccessManager::resourceAccessReset,
        this, &PtzAvailabilityWatcher::updateAvailability);

    connect(m_camera.get(), &nx::vms::client::core::Camera::statusChanged,
        this, &PtzAvailabilityWatcher::updateAvailability);
    connect(m_camera.get(), &nx::vms::client::core::Camera::mediaDewarpingParamsChanged,
        this, &PtzAvailabilityWatcher::updateAvailability);
    connect(m_camera.get(), &nx::vms::client::core::Camera::propertyChanged,
        this, &PtzAvailabilityWatcher::updateAvailability);
}

void PtzAvailabilityWatcher::updateAvailability()
{
    auto nonAvailableSetter = nx::utils::makeScopeGuard(
        [this]() { setAvailable(false); });

    if (!NX_ASSERT(m_camera))
        return;

    const auto systemContext = nx::vms::client::core::SystemContext::fromResource(m_camera);
    if (!NX_ASSERT(systemContext))
        return;

    const auto userWatcher = systemContext->userWatcher();
    const auto user = userWatcher->user();

    if (!user
        || !systemContext->resourceAccessManager()->hasPermission(
            user,
            m_camera,
            Qn::WritePtzPermission))
    {
        return;
    }

    if (!m_camera->isOnline())
        return;

    if (!systemContext->ptzControllerPool()->controller(m_camera))
        return;

    const auto server = m_camera->getParentServer();
    const bool wrongServerVersion = !server
        || server->getVersion() < nx::utils::SoftwareVersion(2, 6);
    if (wrongServerVersion)
        return;

    if (m_camera->getDewarpingParams().enabled)
        return;

    if (m_camera->isPtzRedirected())
        return;

    nonAvailableSetter.disarm();
    setAvailable(true);
}

bool PtzAvailabilityWatcher::available() const
{
    return m_available;
}

void PtzAvailabilityWatcher::setAvailable(bool value)
{
    if (m_available == value)
        return;

    m_available = value;
    emit availabilityChanged();
}

} // namespace mobile
} // namespace client
} // namespace nx
