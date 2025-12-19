// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cameras_grid_helper.h"

#include <QtCore/QTimer>
#include <QtQml/QtQml>
#include <QtQml/QQmlEngine>

#include <core/resource/layout_resource.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/resource/user.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/settings/local_settings.h>
#include <nx/vms/client/mobile/system_context.h>

namespace nx::vms::client::mobile {

namespace {

constexpr std::chrono::seconds kPositionSyncInterval(2);

struct Descriptors
{
    UserDescriptor user;
    LayoutDescriptor layout;
};

// Returns the layout's system context or the current system context if layout is null
// (it is a situation of "All cameras" mode.
SystemContext* systemContextForLayout(const QnLayoutResource* layoutResource)
{
    SystemContext* result = nullptr;
    if (layoutResource)
        result = SystemContext::fromResource(layoutResource);

    if (!result)
        result = appContext()->currentSystemContext();

    NX_ASSERT(result);
    return result;
}

// Returns the user's id or cloud login for a cloud connection.
UserDescriptor userDescriptorForSystemContext(const SystemContext* systemContext)
{
    if (!NX_ASSERT(systemContext))
        return {};

    if (systemContext->mode() == SystemContext::Mode::cloudLayouts)
        return appContext()->cloudStatusWatcher()->cloudLogin();

    const auto user = systemContext->accessController()->user();
    if (!user)
        return {}; //< Possible situation at login.

    return user->getId().toString();
}

// Returns the layout's id or local system id for "All cameras" mode.
LayoutDescriptor layoutDescriptorForLayout(const QnLayoutResource* layoutResource)
{
    if (layoutResource)
        return layoutResource->getId().toString();

    const auto systemContext = systemContextForLayout(layoutResource);
    if (NX_ASSERT(systemContext))
        return systemContext->localSystemId().toString();

    return {};
}

std::optional<Descriptors> getDescriptors(const QnLayoutResource* layoutResource)
{
    const auto systemContext = systemContextForLayout(layoutResource);
    if (!NX_ASSERT(systemContext))
        return {};

    const auto userDescriptor = userDescriptorForSystemContext(systemContext);
    if (userDescriptor.isEmpty())
        return {};

    const auto layoutDescriptor = layoutDescriptorForLayout(layoutResource);
    return Descriptors{.user = userDescriptor, .layout = layoutDescriptor};
}

} //namespace

struct CamerasGridHelper::Private
{
    CamerasGridHelper* q;

    QTimer positionSyncTimer;
    bool positionHasToBeSaved = false;

    UserDescriptor userDescriptor;
    LayoutDescriptor layoutDescriptor;
    int positionValueToSave;

    Private(CamerasGridHelper* owner);
    ~Private();

    void saveLayoutPosition();
};

CamerasGridHelper::Private::Private(CamerasGridHelper* owner)
    : q(owner)
{
    positionSyncTimer.setInterval(kPositionSyncInterval);
    positionSyncTimer.setSingleShot(true);
    connect(&positionSyncTimer, &QTimer::timeout, q,
        [this]()
        {
            if (positionHasToBeSaved)
                saveLayoutPosition();
        });
}

CamerasGridHelper::Private::~Private()
{
    if (positionHasToBeSaved)
        saveLayoutPosition();
}

void CamerasGridHelper::Private::saveLayoutPosition()
{
    appContext()->localSettings()->setCameraLayoutPosition(userDescriptor, layoutDescriptor, positionValueToSave);
    positionHasToBeSaved = false;
    positionSyncTimer.stop();
}

void CamerasGridHelper::registerQmlType()
{
    qmlRegisterType<CamerasGridHelper>("Nx.Mobile", 1, 0, "CamerasGridHelper");
}

CamerasGridHelper::CamerasGridHelper()
    : d(new Private(this))
{
}

CamerasGridHelper::~CamerasGridHelper()
{
}

void CamerasGridHelper::saveTargetColumnsCount(
    const QnLayoutResource* layoutResource, int columnsCount)
{
    if (!NX_ASSERT(columnsCount > 0
        || columnsCount == LocalSettings::kInvalidCameraLayoutCustomColumnsCount))
    {
        return;
    }

    const auto descriptors = getDescriptors(layoutResource);
    if (!descriptors)
        return;

    appContext()->localSettings()->setCameraLayoutCustomColumnCount(
        descriptors->user, descriptors->layout, columnsCount);
}

int CamerasGridHelper::loadTargetColumnsCount(const QnLayoutResource* layoutResource)
{
    const auto descriptors = getDescriptors(layoutResource);
    if (!descriptors)
        return LocalSettings::kInvalidCameraLayoutCustomColumnsCount;

    return appContext()->localSettings()->getCameraLayoutCustomColumnCount(
        descriptors->user, descriptors->layout);
}

void CamerasGridHelper::saveLayoutPosition(const QnLayoutResource* layoutResource, int position)
{
    if (!NX_ASSERT(position >= 0))
        return;

    const auto descriptors = getDescriptors(layoutResource);
    if (!descriptors)
        return;

    if (d->positionHasToBeSaved)
    {
        if (d->userDescriptor != descriptors->user || d->layoutDescriptor != descriptors->layout)
            d->saveLayoutPosition();
    }

    d->userDescriptor = descriptors->user;
    d->layoutDescriptor = descriptors->layout;
    d->positionValueToSave = position;
    d->positionHasToBeSaved = true;

    if (!d->positionSyncTimer.isActive())
        d->positionSyncTimer.start();
}

int CamerasGridHelper::loadLayoutPosition(const QnLayoutResource* layoutResource)
{
    const auto descriptors = getDescriptors(layoutResource);
    if (!descriptors)
        return LocalSettings::kDefaultCameraLayoutPosition;

    return appContext()->localSettings()->getCameraLayoutPosition(
        descriptors->user, descriptors->layout);
}

} // namespace nx::vms::client::mobile
