// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "intercom_manager.h"

#include <api/model/api_ioport_data.h>
#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/client_camera.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_management/resources_changes_manager.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/system_context.h>
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

using namespace std::chrono;

namespace nx::vms::client::desktop {

struct IntercomManager::Private: public QObject
{
    IntercomManager* const q;

    QnUserResourcePtr currentUser;

    Private(IntercomManager* owner):
        q(owner)
    {
        connect(q->resourceAccessProvider(),
            &nx::core::access::ResourceAccessProvider::accessChanged,
            this,
            [this](const QnResourceAccessSubject& subject, const QnResourcePtr& resource)
            {
                auto camera = resource.dynamicCast<QnVirtualCameraResource>();
                if (!camera)
                    return;

                if (q->resourceAccessProvider()->hasAccess(currentUser, resource))
                    tryCreateLayouts({camera});
                else
                    tryRemoveLayouts({camera}, /*localOnly*/ true);
            });

        auto camerasListener = new core::SessionResourcesSignalListener<
            QnVirtualCameraResource>(this);
        camerasListener->setOnAddedHandler(
            [this](const QnVirtualCameraResourceList& cameras)
            {
                tryCreateLayouts(cameras);
            });
        camerasListener->setOnRemovedHandler(
            [this](const QnVirtualCameraResourceList& cameras)
            {
                tryRemoveLayouts(cameras);
            });
        camerasListener->addOnSignalHandler(
            &QnVirtualCameraResource::compatibleEventTypesMaybeChanged,
            [this](const QnVirtualCameraResourcePtr& camera)
            {
                tryCreateLayouts({camera});
            });

        camerasListener->start();

        const nx::vms::client::core::UserWatcher* userWatcher = q->systemContext()->userWatcher();
        currentUser = userWatcher->user();
        connect(userWatcher, &nx::vms::client::core::UserWatcher::userChanged,
            [this](const QnUserResourcePtr& user)
            {
                currentUser = user;
                if (currentUser)
                    tryCreateLayouts(currentUser->resourcePool()->getAllCameras());
            });

        auto currentUserListener = new core::SessionResourcesSignalListener<QnUserResource>(this);
        currentUserListener->setOnRemovedHandler(
            [this](const QnUserResourceList& users)
            {
                if (currentUser && users.contains(currentUser))
                    currentUser.reset();
            });
        currentUserListener->start();
    }

    QnVirtualCameraResourceList getIntercomCameras(
        const QnVirtualCameraResourceList& cameras) const
    {
        return cameras.filtered(
            [](const QnVirtualCameraResourcePtr& camera)
            {
                const auto clientCamera = camera.dynamicCast<QnClientCameraResource>();
                return NX_ASSERT(clientCamera) && clientCamera->isIntercom();
            });
    }

    void tryCreateLayouts(const QnVirtualCameraResourceList& cameras)
    {
        const QnVirtualCameraResourceList intercomCameras = getIntercomCameras(cameras);

        for (const auto& intercom: intercomCameras)
            tryCreateIntercomLocalLayout(intercom);
    }

    void tryRemoveLayouts(const QnVirtualCameraResourceList& cameras, bool localOnly = false)
    {
        const QnVirtualCameraResourceList intercomCameras = getIntercomCameras(cameras);

        for (const auto& intercom: intercomCameras)
            tryRemoveIntercomLayout(intercom, localOnly);
    }

    void tryCreateIntercomLocalLayout(const QnResourcePtr& intercom)
    {
        if (!q->resourceAccessProvider()->hasAccess(currentUser, intercom))
            return;

        auto resourcePool = intercom->resourcePool();

        const QnUuid intercomLayoutId = nx::vms::common::calculateIntercomLayoutId(intercom);
        auto layoutResource = resourcePool->getResourceById(intercomLayoutId);

        if (!layoutResource)
        {
            LayoutResourcePtr intercomLayout = layoutFromResource(intercom);
            intercomLayout->setName(tr("%1 Layout").arg(intercom->getName()));
            intercomLayout->setIdUnsafe(intercomLayoutId);
            intercomLayout->addFlags(Qn::local_intercom_layout);
            intercomLayout->setParentId(intercom->getId());

            auto intercomLayoutItems = intercomLayout->getItems();
            if (NX_ASSERT(intercomLayoutItems.size() == 1))
            {
                const auto intercomItem = intercomLayoutItems.begin();
                intercomItem->uuid = nx::vms::common::calculateIntercomItemId(intercom);
                intercomLayout->setItems(intercomLayoutItems);
            }

            resourcePool->addNewResources({intercomLayout});
        }
    }

    void tryRemoveIntercomLayout(const QnResourcePtr& intercom, bool localOnly)
    {
        auto resourcePool = intercom->resourcePool();

        const QnUuid intercomLayoutId = nx::vms::common::calculateIntercomLayoutId(intercom);
        QnResourcePtr layoutResource = resourcePool->getResourceById(intercomLayoutId);

        if (layoutResource && !layoutResource->hasFlags(Qn::removed))
        {
            if (layoutResource->hasFlags(Qn::local))
                resourcePool->removeResource(layoutResource);
            else if (!localOnly)
                qnResourcesChangesManager->deleteResources({layoutResource});
        }
    }
}; // struct IntercomManager::Private

IntercomManager::IntercomManager(
    nx::vms::client::desktop::SystemContext* systemContext,
    QObject* parent)
    :
    base_type(parent),
    nx::vms::client::desktop::SystemContextAware(systemContext),
    d(new Private(this))
{
}

IntercomManager::~IntercomManager()
{
}

} // namespace nx::vms::client::desktop
