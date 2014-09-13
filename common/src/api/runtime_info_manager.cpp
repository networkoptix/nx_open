#include "runtime_info_manager.h"

#include <api/common_message_processor.h>

#include <common/common_module.h>

#include <nx_ec/data/api_server_alive_data.h>
    
//#define RUNTIME_INFO_DEBUG

QnRuntimeInfoManager::QnRuntimeInfoManager(QObject* parent):
    QObject(parent),
    m_items(new QnThreadsafeItemStorage<QnPeerRuntimeInfo>(&m_mutex, this))
{
    connect( QnCommonMessageProcessor::instance(), &QnCommonMessageProcessor::runtimeInfoChanged, this, [this](const ec2::ApiRuntimeData &runtimeData) 
    {
        QMutexLocker lock(&m_updateMutex);
        QnPeerRuntimeInfo info(runtimeData);
        if (m_items->hasItem(info.uuid)) {
            if (runtimeData.version > m_items->getItem(runtimeData.peer.id).data.version)
                m_items->updateItem(info.uuid, info);
        }
        else
            m_items->addItem(info);
    });

    connect( QnCommonMessageProcessor::instance(), &QnCommonMessageProcessor::remotePeerLost,     this, [this](const ec2::ApiPeerAliveData &data){
        m_items->removeItem(data.peer.id);
    });

    connect( QnCommonMessageProcessor::instance(), &QnCommonMessageProcessor::connectionClosed,   this, [this]{
        m_items->setItems(QnPeerRuntimeInfoList() << localInfo());
    });
}

const QnThreadsafeItemStorage<QnPeerRuntimeInfo> * QnRuntimeInfoManager::items() const {
    return m_items.data();
}


void QnRuntimeInfoManager::storedItemAdded(const QnPeerRuntimeInfo &item) {
#ifdef RUNTIME_INFO_DEBUG
    qDebug() <<"runtime info added" << item.uuid << item.data.peer.peerType;
#endif
    emit runtimeInfoAdded(item);
}

void QnRuntimeInfoManager::storedItemRemoved(const QnPeerRuntimeInfo &item) {
#ifdef RUNTIME_INFO_DEBUG
    qDebug() <<"runtime info removed" << item.uuid << item.data.peer.peerType;
#endif
    emit runtimeInfoRemoved(item);  
}

void QnRuntimeInfoManager::storedItemChanged(const QnPeerRuntimeInfo &item) {
#ifdef RUNTIME_INFO_DEBUG
    qDebug() <<"runtime info changed" << item.uuid << item.data.peer.peerType;
#endif
    emit runtimeInfoChanged(item);
}

QnPeerRuntimeInfo QnRuntimeInfoManager::localInfo() const {
    Q_ASSERT(m_items->hasItem(qnCommon->moduleGUID()));
    return m_items->getItem(qnCommon->moduleGUID());
}

QnPeerRuntimeInfo QnRuntimeInfoManager::item(const QUuid& id) const {
    return m_items->getItem(id);
}

QnPeerRuntimeInfo QnRuntimeInfoManager::remoteInfo() const {
    if (!m_items->hasItem(qnCommon->remoteGUID()))
        return QnPeerRuntimeInfo();
    return m_items->getItem(qnCommon->remoteGUID());
}

bool QnRuntimeInfoManager::hasItem(const QUuid& id)
{
    return m_items->hasItem(id);
}

void QnRuntimeInfoManager::updateLocalItem(const QnPeerRuntimeInfo& value)
{
    QMutexLocker lock(&m_updateMutex);
    if (m_items->hasItem(value.uuid)) {
        int oldVersion = m_items->getItem(value.uuid).data.version;
        QnPeerRuntimeInfo modifiedValue = value;
        modifiedValue.data.version = oldVersion + 1;
        m_items->updateItem(value.uuid, modifiedValue);
    }
    else {
        m_items->addItem(value);
    }
}
