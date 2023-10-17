// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "intercom_manager.h"

#include <api/model/api_ioport_data.h>
#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/client_camera.h>
#include <core/resource/resource_property_key.h>
#include <core/resource/user_resource.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/resource/rest_api_helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/event_parameters.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_event_rules_manager.h>
#include <ui/workbench/workbench_context.h>

using namespace std::chrono;

namespace nx::vms::client::desktop {

// ------------------------------------------------------------------------------------------------
// IntercomManager::Private

struct IntercomManager::Private: public QObject, public SystemContextAware
{
    IntercomManager* const q;

    Private(SystemContext* context, IntercomManager* owner):
        SystemContextAware(context),
        q(owner)
    {
        const auto accessController =
            qobject_cast<CachingAccessController*>(this->accessController());
        NX_ASSERT(accessController);

        connect(accessController, &CachingAccessController::permissionsChanged, this,
            [this](const QnResourcePtr& resource)
            {
                const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
                if (camera && Private::isIntercom(camera))
                    updateLocalLayouts({camera});
            });

        tryCreateLayouts(getAllIntercomCameras());

        const auto camerasListener =
            new core::SessionResourcesSignalListener<QnVirtualCameraResource>(
                systemContext(), this);

        camerasListener->setOnAddedHandler(
            [this](const QnVirtualCameraResourceList& cameras)
            {
                tryCreateLayouts(getIntercomCameras(cameras));
            });

        camerasListener->setOnRemovedHandler(
            [this](const QnVirtualCameraResourceList& cameras)
            {
                tryRemoveLayouts(getIntercomCameras(cameras), /*localOnly*/ false);
            });

        camerasListener->addOnPropertyChangedHandler(
            [this](const QnVirtualCameraResourcePtr& camera, const QString& key)
            {
                if (key == ResourcePropertyKey::kIoSettings && Private::isIntercom(camera))
                    tryCreateLayouts({camera});
            });

        camerasListener->start();
    }

    static bool isIntercom(const QnVirtualCameraResourcePtr& camera)
    {
        const auto clientCamera = camera.dynamicCast<QnClientCameraResource>();
        return NX_ASSERT(clientCamera) && clientCamera->isIntercom();
    }

    QnVirtualCameraResourceList getIntercomCameras(
        const QnVirtualCameraResourceList& cameras) const
    {
        return cameras.filtered(&Private::isIntercom);
    }

    QnVirtualCameraResourceList getAllIntercomCameras() const
    {
        const auto currentUser = accessController()->user();
        return currentUser
            ? getIntercomCameras(resourcePool()->getAllCameras())
            : QnVirtualCameraResourceList{};
    }

    bool hasAccess(const QnVirtualCameraResourcePtr& camera)
    {
        return accessController()->hasPermissions(camera,
            Qn::ViewLivePermission | Qn::TwoWayAudioPermission);
    }

    void tryCreateLayouts(const QnVirtualCameraResourceList& intercomCameras)
    {
        for (const auto& intercom: intercomCameras)
            tryCreateIntercomLocalLayout(intercom);
    }

    void tryRemoveLayouts(
        const QnVirtualCameraResourceList& intercomCameras, bool localOnly)
    {
        for (const auto& intercom: intercomCameras)
            tryRemoveIntercomLayout(intercom, localOnly);
    }

    void updateLocalLayouts(const QnVirtualCameraResourceList& intercomCameras)
    {
        QnVirtualCameraResourceList accessible;
        QnVirtualCameraResourceList inaccessible;

        for (const auto& intercom: intercomCameras)
        {
            auto* target = hasAccess(intercom) ? &accessible : &inaccessible;
            target->push_back(intercom);
        }

        tryRemoveLayouts(inaccessible, /*localOnly*/ true);
        tryCreateLayouts(accessible);
    }

    void tryCreateIntercomLocalLayout(const QnVirtualCameraResourcePtr& intercomCamera)
    {
        if (!NX_ASSERT(Private::isIntercom(intercomCamera)) || !hasAccess(intercomCamera))
            return;

        auto resourcePool = intercomCamera->resourcePool();
        if (!NX_ASSERT(resourcePool == this->resourcePool()))
            return;

        const QnUuid intercomLayoutId = nx::vms::common::calculateIntercomLayoutId(intercomCamera);
        auto layoutResource = resourcePool->getResourceById(intercomLayoutId);

        if (!layoutResource)
        {
            LayoutResourcePtr intercomLayout = layoutFromResource(intercomCamera);
            intercomLayout->setName(tr("%1 Layout").arg(intercomCamera->getName()));
            intercomLayout->setIdUnsafe(intercomLayoutId);
            intercomLayout->addFlags(Qn::local_intercom_layout);
            intercomLayout->setParentId(intercomCamera->getId());

            resourcePool->addNewResources({intercomLayout});
        }
    }

    void tryRemoveIntercomLayout(const QnVirtualCameraResourcePtr& intercomCamera, bool localOnly)
    {
        if (!NX_ASSERT(Private::isIntercom(intercomCamera)))
            return;

        auto resourcePool = intercomCamera->resourcePool();
        if (!NX_ASSERT(resourcePool == this->resourcePool()))
            return;

        const QnUuid intercomLayoutId = nx::vms::common::calculateIntercomLayoutId(intercomCamera);
        QnResourcePtr layoutResource = resourcePool->getResourceById(intercomLayoutId);

        if (layoutResource && !layoutResource->hasFlags(Qn::removed))
        {
            if (layoutResource->hasFlags(Qn::local))
            {
                resourcePool->removeResource(layoutResource);
            }
            else if (!localOnly)
            {
                const auto systemContext = SystemContext::fromResource(layoutResource);
                qnResourcesChangesManager->deleteResource(layoutResource);
            }
        }
    }
};

// ------------------------------------------------------------------------------------------------
// IntercomManager

IntercomManager::IntercomManager(SystemContext* systemContext):
    d(new Private(systemContext, this))
{
}

IntercomManager::~IntercomManager()
{
}

} // namespace nx::vms::client::desktop
