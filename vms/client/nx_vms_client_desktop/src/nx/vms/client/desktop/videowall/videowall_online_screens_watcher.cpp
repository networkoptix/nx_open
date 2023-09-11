// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_online_screens_watcher.h"

#include <api/runtime_info_manager.h>
#include <nx/vms/common/system_context.h>

namespace nx::vms::client::desktop {

VideoWallOnlineScreensWatcher::VideoWallOnlineScreensWatcher(
    nx::vms::common::SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent)
{
    auto setItemOnline =
        [this](const QnPeerRuntimeInfo& info, bool online)
        {
            // Ignore non-videowall clients.
            if (info.data.peer.peerType != vms::api::PeerType::videowallClient)
                return;

            // Ignore master node, it will run other clients and exit.
            if (info.data.videoWallInstanceGuid.isNull())
                return;

            const auto& instanceGuid = info.data.videoWallInstanceGuid;
            if (online)
                m_onlineScreens.insert(instanceGuid);
            else
                m_onlineScreens.erase(instanceGuid);
            emit onlineScreensChanged();
        };

    auto runtimeInfoManager = systemContext->runtimeInfoManager();
    connect(runtimeInfoManager, &QnRuntimeInfoManager::runtimeInfoAdded, this,
        [this, setItemOnline](const QnPeerRuntimeInfo& info)
        {
            setItemOnline(info, true);
        });

    connect(runtimeInfoManager, &QnRuntimeInfoManager::runtimeInfoRemoved, this,
        [this, setItemOnline](const QnPeerRuntimeInfo& info)
        {
            setItemOnline(info, false);
        });

    for (const auto& info: runtimeInfoManager->items()->getItems())
        setItemOnline(info, true);
}

std::set<QnUuid> VideoWallOnlineScreensWatcher::onlineScreens() const
{
    return m_onlineScreens;
}

} // namespace nx::vms::client::desktop
