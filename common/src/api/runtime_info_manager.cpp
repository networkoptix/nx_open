#include "runtime_info_manager.h"

#include <api/common_message_processor.h>

#include <common/common_module.h>

#include <nx_ec/data/api_server_alive_data.h>
    
QnRuntimeInfoManager::QnRuntimeInfoManager(QObject* parent):
    QObject(parent)
{
    connect( QnCommonMessageProcessor::instance(), &QnCommonMessageProcessor::runtimeInfoChanged, this, &QnRuntimeInfoManager::at_runtimeInfoChanged );
    connect( QnCommonMessageProcessor::instance(), &QnCommonMessageProcessor::remotePeerLost,     this, [this](const ec2::ApiPeerAliveData &data, bool){
        m_items->removeItem(data.peer.id);
    });
    connect( QnCommonMessageProcessor::instance(), &QnCommonMessageProcessor::connectionClosed,   this, [this]{
        m_items->setItems(QnPeerRuntimeInfoList() << localInfo());
    });
}

QnThreadsafeItemStorage<QnPeerRuntimeInfo> * QnRuntimeInfoManager::items() const {
    return m_items.data();
}

void QnRuntimeInfoManager::at_runtimeInfoChanged(const ec2::ApiRuntimeData &runtimeData) {
    
    // check info version
    if (m_items->hasItem(runtimeData.peer.id)) {
        QnPeerRuntimeInfo existingInfo = m_items->getItem(runtimeData.peer.id);
        if (existingInfo.data.version >= runtimeData.version)
            return;
    }

    QnPeerRuntimeInfo info(runtimeData);
    m_items->addItem(info);
}

void QnRuntimeInfoManager::storedItemAdded(const QnPeerRuntimeInfo &item) {
    emit runtimeInfoAdded(item);
}

void QnRuntimeInfoManager::storedItemRemoved(const QnPeerRuntimeInfo &item) {
    emit runtimeInfoRemoved(item);  
}

void QnRuntimeInfoManager::storedItemChanged(const QnPeerRuntimeInfo &item) {
    emit runtimeInfoChanged(item);
}

QnPeerRuntimeInfo QnRuntimeInfoManager::localInfo() const {
    Q_ASSERT(m_items->hasItem(qnCommon->moduleGUID()));
    return m_items->getItem(qnCommon->moduleGUID());
}

QnPeerRuntimeInfo QnRuntimeInfoManager::remoteInfo() const {
    if (!m_items->hasItem(qnCommon->remoteGUID()))
        return QnPeerRuntimeInfo();
    return m_items->getItem(qnCommon->remoteGUID());
}

