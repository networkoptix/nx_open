// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "runtime_info_manager.h"

#include <nx/utils/log/log.h>

#include <api/common_message_processor.h>

#include <common/common_module.h>

//#define RUNTIME_INFO_DEBUG

QnRuntimeInfoManager::QnRuntimeInfoManager(QObject* parent):
    QObject(parent),
    QnCommonModuleAware(parent),
    m_items(new QnThreadsafeItemStorage<QnPeerRuntimeInfo>(&m_mutex, this))
{
}

void QnRuntimeInfoManager::setMessageProcessor(QnCommonMessageProcessor* messageProcessor)
{
    if (m_messageProcessor)
        disconnect(m_messageProcessor, nullptr, this, nullptr);

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
            [this](QnUuid peer, nx::vms::api::PeerType peerType)
            {
                NX_DEBUG(this, "Remote peer lost: id %1, type %2", peer, peerType);
                m_items->removeItem(peer);
            });

        connect(messageProcessor, &QnCommonMessageProcessor::connectionClosed, this,
            [this]()
            {
                m_items->setItems(QnPeerRuntimeInfoList() << localInfo());
            });

        /* Client updates running instance guid on each connect to server */
        connect(commonModule(), &QnCommonModule::runningInstanceGUIDChanged, this,
            [this]()
            {
                nx::vms::api::RuntimeData item = localInfo().data;
                item.peer.instanceId = commonModule()->runningInstanceGUID();
                updateLocalItem(item);
            }, Qt::DirectConnection);
    }
    m_messageProcessor = messageProcessor;
}

const QnThreadsafeItemStorage<QnPeerRuntimeInfo> * QnRuntimeInfoManager::items() const
{
    return m_items.data();
}

Qn::Notifier QnRuntimeInfoManager::storedItemAdded(const QnPeerRuntimeInfo& item)
{
#ifdef RUNTIME_INFO_DEBUG
    qDebug() <<"runtime info added" << item.uuid << item.data.peer.peerType;
#endif
    return [this, item](){ emit runtimeInfoAdded(item); };
}

Qn::Notifier QnRuntimeInfoManager::storedItemRemoved(const QnPeerRuntimeInfo& item)
{
#ifdef RUNTIME_INFO_DEBUG
    qDebug() <<"runtime info removed" << item.uuid << item.data.peer.peerType;
#endif
    return [this, item](){ emit runtimeInfoRemoved(item); };
}

Qn::Notifier QnRuntimeInfoManager::storedItemChanged(const QnPeerRuntimeInfo& item)
{
#ifdef RUNTIME_INFO_DEBUG
    qDebug() <<"runtime info changed" << item.uuid << item.data.peer.peerType;
#endif
    return [this, item]{ emit runtimeInfoChanged(item); };
}

QnPeerRuntimeInfo QnRuntimeInfoManager::localInfo() const
{
    NX_ASSERT(m_items->hasItem(commonModule()->moduleGUID()));
    return m_items->getItem(commonModule()->moduleGUID());
}

QnPeerRuntimeInfo QnRuntimeInfoManager::item(const QnUuid& id) const
{
    return m_items->getItem(id);
}

QnPeerRuntimeInfo QnRuntimeInfoManager::remoteInfo() const
{
    if (!m_items->hasItem(commonModule()->remoteGUID()))
        return QnPeerRuntimeInfo();
    return m_items->getItem(commonModule()->remoteGUID());
}

bool QnRuntimeInfoManager::hasItem(const QnUuid& id)
{
    return m_items->hasItem(id);
}

void QnRuntimeInfoManager::updateLocalItem(const QnPeerRuntimeInfo& value)
{
    NX_MUTEX_LOCKER lock( &m_updateMutex );

    NX_ASSERT(value.uuid == commonModule()->moduleGUID());
    updateItem(value);
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
