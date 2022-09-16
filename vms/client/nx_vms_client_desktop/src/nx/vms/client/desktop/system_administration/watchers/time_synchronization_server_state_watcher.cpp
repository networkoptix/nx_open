// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_synchronization_server_state_watcher.h"

#include <QtCore/QPointer>

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>

#include "../flux/time_synchronization_widget_store.h"

namespace nx::vms::client::desktop
{

class TimeSynchronizationServerStateWatcher::Private : public QObject
{
    TimeSynchronizationServerStateWatcher* q = nullptr;

public:
    Private(TimeSynchronizationServerStateWatcher* q, TimeSynchronizationWidgetStore* store) :
        QObject(),
        q(q),
        m_store(store)
    {
        NX_ASSERT(store);

        for (const auto& server: q->resourcePool()->servers())
        {
            onServerAddedInternal(server);
        }

        connect(q->resourcePool(), &QnResourcePool::resourceAdded,
            this, &TimeSynchronizationServerStateWatcher::Private::onServerAdded);

        connect(q->resourcePool(), &QnResourcePool::resourceRemoved,
            this, &TimeSynchronizationServerStateWatcher::Private::onServerRemoved);
    }

private:
    void onServerAdded(const QnResourcePtr& resource)
    {
        const auto& server = resource.dynamicCast<QnMediaServerResource>();
        if (server && !server->hasFlags(Qn::fake))
        {
            onServerAddedInternal(server);

            TimeSynchronizationWidgetState::ServerInfo serverInfo;
            serverInfo.id = server->getId();
            serverInfo.name = server->getName();
            serverInfo.ipAddress = QnResourceDisplayInfo(server).host();
            serverInfo.online = server->getStatus() == nx::vms::api::ResourceStatus::online;
            serverInfo.hasInternet = server->hasInternetAccess();
            serverInfo.timeZoneOffset = milliseconds(server->utcOffset());

            m_store->addServer(serverInfo);
        }
    }

    void onServerAddedInternal(const QnMediaServerResourcePtr& server)
    {
        NX_ASSERT(server && !server->hasFlags(Qn::fake));

        connect(server.get(), &QnMediaServerResource::statusChanged,
            this, &TimeSynchronizationServerStateWatcher::Private::onStatusChanged);

        connect(server.get(), &QnMediaServerResource::flagsChanged,
            this, &TimeSynchronizationServerStateWatcher::Private::onFlagsChanged);
    }

    void onServerRemoved(const QnResourcePtr& resource)
    {
        const auto& server = resource.dynamicCast<QnMediaServerResource>();
        if (server && !server->hasFlags(Qn::fake))
        {
            server->disconnect(this);
            m_store->removeServer(server->getId());
        }
    }

    void onStatusChanged(const QnResourcePtr& resource)
    {
        const auto& server = resource.dynamicCast<QnMediaServerResource>();
        NX_ASSERT(server && !server->hasFlags(Qn::fake));

        m_store->setServerOnline(
            server->getId(), server->getStatus() == nx::vms::api::ResourceStatus::online);
    }

    void onFlagsChanged(const QnResourcePtr& resource)
    {
        const auto& server = resource.dynamicCast<QnMediaServerResource>();
        NX_ASSERT(server && !server->hasFlags(Qn::fake));

        m_store->setServerOnline(server->getId(), server->hasInternetAccess());
    }

private:
    QPointer<TimeSynchronizationWidgetStore> m_store;
};

TimeSynchronizationServerStateWatcher::TimeSynchronizationServerStateWatcher(
    TimeSynchronizationWidgetStore* store,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, store))
{
}

TimeSynchronizationServerStateWatcher::~TimeSynchronizationServerStateWatcher()
{
}

} // namespace nx::vms::client::desktop
