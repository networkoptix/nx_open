// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "runtime_info_manager.h"

#include <api/common_message_processor.h>
#include <nx/utils/log/log.h>

QnRuntimeInfoManager::QnRuntimeInfoManager(QObject* parent):
    QObject(parent),
    m_items(new QnThreadsafeItemStorage<QnPeerRuntimeInfo>(&m_mutex, this))
{
}

void QnRuntimeInfoManager::setMessageProcessor(QnCommonMessageProcessor* messageProcessor)
{
    if (m_messageProcessor)
        m_messageProcessor->disconnect(this);

    if (messageProcessor)
    {
        connect(messageProcessor, &QnCommonMessageProcessor::runtimeInfoChanged, this,
            [this](const nx::vms::api::RuntimeData &runtimeData)
            {
                NX_DEBUG(this, "Remote peer info update: id %1, type %2",
                    runtimeData.peer.id, runtimeData.peer.peerType);
                QnPeerRuntimeInfo info(runtimeData);
                m_items->addOrUpdateItem(info);
            });

        connect(messageProcessor, &QnCommonMessageProcessor::runtimeInfoRemoved, this,
            [this](const nx::vms::api::IdData& peerId)
            {
                NX_DEBUG(this, "Remote peer info removed: id %1", peerId.id);
                m_items->removeItem(peerId.id);
            });

        connect(messageProcessor, &QnCommonMessageProcessor::remotePeerLost, this,
            [this](nx::Uuid peer, nx::vms::api::PeerType peerType)
            {
                NX_DEBUG(this, "Remote peer lost: id %1, type %2", peer, peerType);
                m_items->removeItem(peer);
            });

        connect(messageProcessor, &QnCommonMessageProcessor::connectionClosed, this,
            [this]()
            {
                m_items->setItems(QnPeerRuntimeInfoList() << localInfo());
            });
    }
    m_messageProcessor = messageProcessor;
}

const QnThreadsafeItemStorage<QnPeerRuntimeInfo> * QnRuntimeInfoManager::items() const
{
    return m_items.data();
}

Qn::Notifier QnRuntimeInfoManager::storedItemAdded(const QnPeerRuntimeInfo& item)
{
    NX_VERBOSE(this, "Runtime info added: %1 [%2]",  item.data.peer.peerType, item.uuid);
    return [this, item](){ emit runtimeInfoAdded(item); };
}

Qn::Notifier QnRuntimeInfoManager::storedItemRemoved(const QnPeerRuntimeInfo& item)
{
    NX_VERBOSE(this, "Runtime info removed: %1 [%2]",  item.data.peer.peerType, item.uuid);
    return [this, item](){ emit runtimeInfoRemoved(item); };
}

Qn::Notifier QnRuntimeInfoManager::storedItemChanged(
    const QnPeerRuntimeInfo& item,
    const QnPeerRuntimeInfo& /*oldItem*/)
{
    NX_VERBOSE(this, "Runtime info changed: %1 [%2]",  item.data.peer.peerType, item.uuid);
    return [this, item]{ emit runtimeInfoChanged(item); };
}

QnPeerRuntimeInfo QnRuntimeInfoManager::localInfo() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_localInfo;
}

QnPeerRuntimeInfo QnRuntimeInfoManager::item(const nx::Uuid& id) const
{
    return m_items->getItem(id);
}

bool QnRuntimeInfoManager::hasItem(const nx::Uuid& id)
{
    return m_items->hasItem(id);
}

void QnRuntimeInfoManager::updateLocalItem(const QnPeerRuntimeInfo& value)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_localInfo = value;
    }

    {
        NX_MUTEX_LOCKER lock( &m_updateMutex );
        updateItem(value);
    }
}

void QnRuntimeInfoManager::updateRemoteItem(const QnPeerRuntimeInfo& value)
{
    NX_MUTEX_LOCKER lock(&m_updateMutex);

    updateItem(value);
}

void QnRuntimeInfoManager::updateItem(const QnPeerRuntimeInfo& value)
{
    QnPeerRuntimeInfo modifiedValue = value;
    if (m_items->hasItem(value.uuid))
    {
        int oldVersion = m_items->getItem(value.uuid).data.version;
        modifiedValue.data.version = oldVersion + 1;
        m_items->updateItem(modifiedValue);
    }
    else
    {
        modifiedValue.data.version = 1;
        m_items->addItem(modifiedValue);
    }
}
