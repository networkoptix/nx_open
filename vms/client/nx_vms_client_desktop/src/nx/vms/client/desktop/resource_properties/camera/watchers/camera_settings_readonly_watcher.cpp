// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_readonly_watcher.h"

#include <QtCore/QPointer>

#include <core/resource/camera_resource.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_utils.h>
#include <nx/vms/common/system_settings.h>
#include <ui/workbench/workbench_context.h>

#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

CameraSettingsReadOnlyWatcher::CameraSettingsReadOnlyWatcher(
    CameraSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    NX_ASSERT(store);

    connect(this, &CameraSettingsReadOnlyWatcher::readOnlyChanged,
        store, &CameraSettingsDialogStore::setReadOnly);

    connect(context(), &QnWorkbenchContext::userChanged, this,
        &CameraSettingsReadOnlyWatcher::updateReadOnly);

    connect(systemContext()->saasServiceManager(),
        &nx::vms::common::saas::ServiceManager::dataChanged,
        this,
        &CameraSettingsReadOnlyWatcher::updateReadOnly);

    connect(systemContext()->globalSettings(),
        &nx::vms::common::SystemSettings::organizationIdChanged,
        this,
        &CameraSettingsReadOnlyWatcher::updateReadOnly);

    updateReadOnly();
}

QnVirtualCameraResourceList CameraSettingsReadOnlyWatcher::cameras() const
{
    return m_cameras;
}

void CameraSettingsReadOnlyWatcher::setCameras(const QnVirtualCameraResourceList& value)
{
    if (m_cameras == value)
        return;

    m_cameras = value;
    updateReadOnly();
}

bool CameraSettingsReadOnlyWatcher::isReadOnly() const
{
    return m_readOnly;
}

void CameraSettingsReadOnlyWatcher::updateReadOnly()
{
    const auto value = calculateReadOnly();
    if (m_readOnly == value)
        return;

    m_readOnly = value;
    emit readOnlyChanged(value, QPrivateSignal());
}

bool CameraSettingsReadOnlyWatcher::calculateReadOnly() const
{
    if (nx::vms::common::saas::saasInitialized(systemContext()))
    {
        const auto serviceManager = systemContext()->saasServiceManager();
        if (serviceManager->saasShutDown())
            return true;
    }

    return std::any_of(m_cameras.cbegin(), m_cameras.cend(),
        [](const QnVirtualCameraResourcePtr& camera)
        {
            return !ResourceAccessManager::hasPermissions(camera, Qn::WritePermission);
        });
}

} // namespace nx::vms::client::desktop
