// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "caching_access_controller.h"

#include <core/resource/user_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/system_context.h>

namespace nx::vms::client::desktop {

using namespace nx::core::access;
using namespace nx::vms::api;

class CachingAccessController::Private: public QObject
{
    CachingAccessController* const q;
    QHash<QnResourcePtr, std::shared_ptr<Notifier>> notifiers;

public:
    explicit Private(CachingAccessController* q);

private:
    void handleResourcesAdded(const QnResourceList& resources);
    void handleResourcesRemoved(const QnResourceList& resources);
};

// ------------------------------------------------------------------------------------------------
// CachingAccessController

CachingAccessController::CachingAccessController(SystemContext* systemContext, QObject* parent):
    base_type(systemContext, parent),
    d(new Private(this))
{
}

CachingAccessController::~CachingAccessController()
{
    // Required here for forward declared scoped pointer destruction.
}

// ------------------------------------------------------------------------------------------------
// CachingAccessController::Private

CachingAccessController::Private::Private(CachingAccessController* q):
    q(q)
{
    const auto resourcePool = q->systemContext()->resourcePool();

    connect(resourcePool, &QnResourcePool::resourcesAdded,
        this, &Private::handleResourcesAdded);

    connect(resourcePool, &QnResourcePool::resourcesRemoved,
        this, &Private::handleResourcesRemoved);

    handleResourcesAdded(resourcePool->getResources());
}

void CachingAccessController::Private::handleResourcesAdded(const QnResourceList& resources)
{
    NX_VERBOSE(q, "Resources added, subscribing for notifications: %1", resources);

    for (const auto& resource: resources)
    {
        const auto notifier = q->createNotifier(resource);
        notifiers.insert(resource, notifier);

        connect(notifier.get(), &Notifier::permissionsChanged, this,
            [this](const QnResourcePtr& resource, Qn::Permissions old, Qn::Permissions current)
            {
                q->permissionsChanged(resource, old, current,
                    CachingAccessController::QPrivateSignal{});
            });
    }
}

void CachingAccessController::Private::handleResourcesRemoved(const QnResourceList& resources)
{
    NX_VERBOSE(q, "Resources removed, unsubscribing from notifications: %1", resources);

    for (const auto& resource: resources)
        notifiers.remove(resource);
}

} // namespace nx::vms::client::desktop
