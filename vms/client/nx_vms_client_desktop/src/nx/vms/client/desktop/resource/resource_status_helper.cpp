// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_status_helper.h"

#include <memory>

#include <QtCore/QPointer>
#include <QtQml/QtQml>
#include <QtWidgets/QAction>

#include <api/runtime_info_manager.h>
#include <client/client_module.h>
#include <client/client_runtime_settings.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource_property_key.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resources_changes_manager.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/license/videowall_license_validator.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/resource_properties/camera/camera_settings_tab.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_health/default_password_cameras_watcher.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/videowall/videowall_online_screens_watcher.h>
#include <nx/vms/license/usage_helper.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/workbench/handlers/workbench_videowall_handler.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

class ResourceStatusHelper::Private: public QObject
{
    ResourceStatusHelper* const q;

public:
    Private(ResourceStatusHelper* q): q(q) {}
    virtual ~Private() override = default;

    QnWorkbenchContext* context() const { return m_context; }
    void setContext(QnWorkbenchContext* value);

    QnResource* resource() const { return m_resource.get(); }
    ResourceType resourceType() const;
    void setResource(QnResource* value);

    StatusFlags status() const { return m_status; }
    LicenseUsage licenseUsage() const { return m_licenseUsage; }

    void executeAction(ActionType type);

private:
    void updateStatus();
    void updateLicenseUsage();
    void setLicenseUsage(LicenseUsage value);
    void setStatus(StatusFlags value);
    bool isVideoWallLicenseValid() const;

private:
    QPointer<QnWorkbenchContext> m_context;
    QnResourcePtr m_resource;
    QnVirtualCameraResourcePtr m_camera;
    std::unique_ptr<nx::vms::license::SingleCamLicenseStatusHelper> m_licenseHelper;
    nx::utils::ScopedConnections m_contextConnections;
    nx::utils::ScopedConnections m_resourceConnections;
    StatusFlags m_status{};
    LicenseUsage m_licenseUsage = LicenseUsage::notUsed;
};

//-------------------------------------------------------------------------------------------------
// ResourceStatusHelper

ResourceStatusHelper::ResourceStatusHelper(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

ResourceStatusHelper::~ResourceStatusHelper()
{
    // Required here for forward-declared scoped pointer destruction.
}

QnWorkbenchContext* ResourceStatusHelper::context() const
{
    return d->context();
}

void ResourceStatusHelper::setContext(QnWorkbenchContext* value)
{
    d->setContext(value);
}

QnResource* ResourceStatusHelper::resource() const
{
    return d->resource();
}

void ResourceStatusHelper::setResource(QnResource* value)
{
    d->setResource(value);
}

ResourceStatusHelper::StatusFlags ResourceStatusHelper::statusFlags() const
{
    return d->status();
}

ResourceStatusHelper::ResourceType ResourceStatusHelper::resourceType() const
{
    return d->resourceType();
}

ResourceStatusHelper::LicenseUsage ResourceStatusHelper::licenseUsage() const
{
    return d->licenseUsage();
}

int ResourceStatusHelper::defaultPasswordCameras() const
{
    if (!d->context())
        return 0;

    const auto watcher = d->context()->findInstance<DefaultPasswordCamerasWatcher>();
    return watcher ? watcher->camerasWithDefaultPassword().size() : 0;
}

void ResourceStatusHelper::executeAction(ActionType type)
{
    d->executeAction(type);
}

void ResourceStatusHelper::registerQmlType()
{
    qmlRegisterType<ResourceStatusHelper>("nx.vms.client.desktop", 1, 0, "ResourceStatusHelper");
}

//-------------------------------------------------------------------------------------------------
// ResourceStatusHelper::Private

void ResourceStatusHelper::Private::setContext(QnWorkbenchContext* value)
{
    if (m_context == value)
        return;

    m_contextConnections.reset();
    m_context = value;

    if (m_context)
    {
        const auto handlePermissionsChanged =
            [this](const QnResourcePtr& resource)
            {
                if (resource == m_camera)
                    updateStatus();
            };

        m_contextConnections << connect(m_context->accessController(),
            &QnWorkbenchAccessController::permissionsChanged,
            this,
            handlePermissionsChanged);

        m_contextConnections << connect(m_context->accessController(),
            &QnWorkbenchAccessController::globalPermissionsChanged,
            this,
            &Private::updateStatus);

        m_contextConnections << connect(m_context->action(ui::action::ToggleLayoutTourModeAction),
            &QAction::toggled,
            this,
            &Private::updateStatus);

        const auto watcher = m_context->findInstance<DefaultPasswordCamerasWatcher>();
        if (NX_ASSERT(watcher))
        {
            m_contextConnections << connect(watcher,
                &DefaultPasswordCamerasWatcher::cameraSetChanged,
                q,
                &ResourceStatusHelper::defaultPasswordCamerasChanged);
        }
    }

    emit q->contextChanged();
    emit q->defaultPasswordCamerasChanged();

    updateStatus();
}

ResourceStatusHelper::ResourceType ResourceStatusHelper::Private::resourceType() const
{
    if (!m_resource)
        return ResourceType::none;

    if (m_camera)
        return m_camera->isIOModule() ? ResourceType::ioModule : ResourceType::camera;

    if (m_resource->hasFlags(Qn::local_media))
        return ResourceType::localMedia;

    return ResourceType::other;
}

void ResourceStatusHelper::Private::setResource(QnResource* value)
{
    if (value == m_resource)
        return;

    m_resourceConnections.reset();
    m_licenseHelper.reset();

    m_resource = value ? value->toSharedPointer() : QnResourcePtr();
    m_camera = m_resource.objectCast<QnVirtualCameraResource>();

    if (m_resource)
    {
        m_resourceConnections << connect(m_resource.get(), &QnResource::statusChanged,
            this, &Private::updateStatus);

        m_resourceConnections << connect(m_resource.get(), &QnResource::propertyChanged, this,
            [this](const QnResourcePtr& /*resource*/, const QString& key)
            {
                static const QSet<QString> kHandledProperties = {
                    ResourcePropertyKey::kCameraCapabilities,
                    ResourcePropertyKey::kDts };

                if (kHandledProperties.contains(key))
                    updateStatus();
            });

        if (m_camera)
        {
            using namespace nx::vms::license;
            m_licenseHelper.reset(new SingleCamLicenseStatusHelper(m_camera));
            connect(m_licenseHelper.get(), &SingleCamLicenseStatusHelper::licenseStatusChanged,
                this, &Private::updateLicenseUsage);
        }

        auto systemContext = SystemContext::fromResource(m_resource);
        m_resourceConnections << connect(systemContext->videoWallOnlineScreensWatcher(),
            &VideoWallOnlineScreensWatcher::onlineScreensChanged,
            this,
            &Private::updateStatus);
    }

    emit q->resourceChanged();

    updateLicenseUsage();
    updateStatus();
}

void ResourceStatusHelper::Private::updateStatus()
{
    if (!m_resource || !m_context)
    {
        setStatus({});
        return;
    }

    StatusFlags status{};
    m_status.setFlag(StatusFlag::videowallLicenseRequired,
        qnRuntime->isVideoWallMode() && !isVideoWallLicenseValid());

    if (m_camera)
    {
        status.setFlag(StatusFlag::noLiveStream, m_camera->hasFlags(Qn::virtual_camera));
        status.setFlag(StatusFlag::defaultPassword, m_camera->needsToChangeDefaultPassword());
        status.setFlag(StatusFlag::oldFirmware,
            m_camera->hasCameraCapabilities(nx::vms::api::DeviceCapability::isOldFirmware));

        status.setFlag(StatusFlag::canViewLive,
            ResourceAccessManager::hasPermissions(m_resource, Qn::ViewLivePermission));

        status.setFlag(StatusFlag::canViewArchive,
            ResourceAccessManager::hasPermissions(m_resource, Qn::ViewFootagePermission));

        status.setFlag(StatusFlag::canEditSettings,
            ResourceAccessManager::hasPermissions(m_resource,
                Qn::SavePermission | Qn::WritePermission));

        status.setFlag(StatusFlag::canInvokeDiagnostics, m_camera->hasFlags(Qn::live_cam)
            && !m_context->action(ui::action::ToggleLayoutTourModeAction)->isChecked());

        status.setFlag(StatusFlag::dts, m_camera->isDtsBased());

        status.setFlag(StatusFlag::accessDenied,
            !ResourceAccessManager::hasPermissions(m_camera, Qn::ViewContentPermission));
    }

    status.setFlag(StatusFlag::canChangePasswords, qnRuntime->isDesktopMode()
        && m_context->accessController()->hasGlobalPermission(
            GlobalPermission::admin));

    const auto resourceStatus = m_resource->getStatus();
    status.setFlag(StatusFlag::offline, resourceStatus == nx::vms::api::ResourceStatus::offline);
    status.setFlag(StatusFlag::unauthorized, resourceStatus == nx::vms::api::ResourceStatus::unauthorized);
    status.setFlag(StatusFlag::mismatchedCertificate, resourceStatus == nx::vms::api::ResourceStatus::mismatchedCertificate);

    if (auto mediaResource = m_resource.dynamicCast<QnMediaResource>())
        status.setFlag(StatusFlag::noVideoData, !mediaResource->hasVideo());

    setStatus(status);
}

void ResourceStatusHelper::Private::setStatus(StatusFlags value)
{
    if (m_status == value)
        return;

    m_status = value;
    emit q->statusChanged();
}

void ResourceStatusHelper::Private::updateLicenseUsage()
{
    const auto usage =
        [](nx::vms::license::UsageStatus status) -> LicenseUsage
        {
            switch (status)
            {
                case nx::vms::license::UsageStatus::invalid:
                case nx::vms::license::UsageStatus::notUsed:
                    return LicenseUsage::notUsed;

                case nx::vms::license::UsageStatus::used:
                    return LicenseUsage::used;

                case nx::vms::license::UsageStatus::overflow:
                    return LicenseUsage::overflow;

                default:
                    NX_ASSERT(false);
                    return LicenseUsage::notUsed;
            }
        };

    setLicenseUsage(m_licenseHelper
        ? usage(m_licenseHelper->status())
        : LicenseUsage::notUsed);
}

void ResourceStatusHelper::Private::setLicenseUsage(LicenseUsage value)
{
    if (m_licenseUsage == value)
        return;

    m_licenseUsage = value;
    emit q->licenseUsageChanged();
}

void ResourceStatusHelper::Private::executeAction(ActionType type)
{
    if (!NX_ASSERT(m_context))
        return;

    const auto changeCameraPassword =
        [this](const QnVirtualCameraResourceList& cameras, bool forceShowCamerasList)
        {
            auto parameters = ui::action::Parameters(cameras);
            parameters.setArgument(Qn::ForceShowCamerasList, forceShowCamerasList);
            m_context->menu()->trigger(ui::action::ChangeDefaultCameraPasswordAction, parameters);
        };

    switch (type)
    {
        case ActionType::diagnostics:
        {
            statisticsModule()->controls()->registerClick(
                "resource_status_overlay_diagnostics");
            if (NX_ASSERT(m_camera))
                m_context->menu()->trigger(ui::action::CameraDiagnosticsAction, m_camera);

            break;
        }

        case ActionType::enableLicense:
        {
            statisticsModule()->controls()->registerClick(
                "resource_status_overlay_enable_license");
            if (!NX_ASSERT(m_camera && m_licenseUsage == LicenseUsage::notUsed))
                return;

            qnResourcesChangesManager->saveCamera(m_camera,
                [](const QnVirtualCameraResourcePtr& camera)
                {
                    camera->setScheduleEnabled(true);
                });

            break;
        }

        case ActionType::moreLicenses:
        {
            statisticsModule()->controls()->registerClick(
                "resource_status_overlay_more_licenses");
            m_context->menu()->trigger(ui::action::PreferencesLicensesTabAction);
            break;
        }

        case ActionType::settings:
        {
            statisticsModule()->controls()->registerClick(
                "resource_status_overlay_settings");
            if (!NX_ASSERT(m_camera))
                break;

            m_context->menu()->trigger(ui::action::CameraSettingsAction,
                ui::action::Parameters(m_camera).withArgument(Qn::FocusTabRole,
                    CameraSettingsTab::general));
            break;
        }

        case ActionType::setPassword:
        {
            if (NX_ASSERT(m_camera))
                changeCameraPassword({m_camera}, false);

            break;
        }

        case ActionType::setPasswords:
        {
            const auto watcher = m_context->findInstance<DefaultPasswordCamerasWatcher>();
            if (NX_ASSERT(watcher))
                changeCameraPassword(watcher->camerasWithDefaultPassword().values(), true);

            break;
        }
    }
}

bool ResourceStatusHelper::Private::isVideoWallLicenseValid() const
{
    if (!NX_ASSERT(m_resource && qnRuntime->isVideoWallMode()))
        return true;

    auto systemContext = SystemContext::fromResource(m_resource);

    auto helper = qnClientModule->videoWallLicenseUsageHelper();
    if (helper->isValid())
        return true;

    const QnPeerRuntimeInfo localInfo = systemContext->runtimeInfoManager()->localInfo();
    NX_ASSERT(localInfo.data.peer.peerType == nx::vms::api::PeerType::videowallClient);
    QnUuid currentScreenId = localInfo.data.videoWallInstanceGuid;

    // Gather all online screen ids.
    // The order of screens should be the same across all client instances.
    const auto onlineScreens = systemContext->videoWallOnlineScreensWatcher()->onlineScreens();

    const int allowedLiceses = helper->totalLicenses(Qn::LC_VideoWall);

    // Calculate the number of screens that should show invalid license overlay.
    using Helper = nx::vms::license::VideoWallLicenseUsageHelper;
    size_t sceensToDisable = 0;
    while (sceensToDisable < onlineScreens.size()
        && Helper::licensesForScreens(onlineScreens.size() - sceensToDisable) > allowedLiceses)
    {
        ++sceensToDisable;
    }

    if (sceensToDisable > 0)
    {
        // If current screen falls into the range - report invalid license.
        const size_t i = std::distance(onlineScreens.begin(), onlineScreens.find(currentScreenId));
        if (i < sceensToDisable)
            return false;
    }

    return true;
}

} // namespace nx::vms::client::desktop
