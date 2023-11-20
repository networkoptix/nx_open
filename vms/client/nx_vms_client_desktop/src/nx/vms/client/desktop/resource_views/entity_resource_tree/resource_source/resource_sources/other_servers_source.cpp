// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "other_servers_source.h"

#include <nx/vms/client/desktop/other_servers/other_servers_manager.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

OtherServersSource::OtherServersSource(
    const OtherServersManager* otherServersManager,
    QObject* parent)
    :
    QObject(parent),
    entity_item_model::UniqueKeySource<QnUuid>(),
    m_otherServerManager(otherServersManager)
{
    if (!NX_ASSERT(m_otherServerManager))
        return;

    initializeRequest =
        [this]
        {
            setKeysHandler(m_otherServerManager->getServers());

            m_connectionsGuard.add(QObject::connect(
                m_otherServerManager, &OtherServersManager::serverAdded,
                this, *addKeyHandler));

            m_connectionsGuard.add(QObject::connect(
                m_otherServerManager, &OtherServersManager::serverRemoved,
                this, *removeKeyHandler));
        };
}

OtherServersSource::~OtherServersSource()
{
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
