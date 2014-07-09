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
    if (item.uuid != qnCommon->remoteGUID())
        return;

    qnLicensePool->setMainHardwareIds(item.data.mainHardwareIds);
    qnLicensePool->setCompatibleHardwareIds(item.data.compatibleHardwareIds);
}

void QnRuntimeInfoManager::storedItemRemoved(const QnPeerRuntimeInfo &item) {
    emit runtimeInfoRemoved(item);
    if (item.uuid != qnCommon->remoteGUID())
        return;

    qnLicensePool->setMainHardwareIds(QList<QByteArray>());
    qnLicensePool->setCompatibleHardwareIds(QList<QByteArray>());

    //clear all info (but self) on disconnect
    m_items->setItems(QnPeerRuntimeInfoList() << localInfo());
}

void QnRuntimeInfoManager::storedItemChanged(const QnPeerRuntimeInfo &item) {
    emit runtimeInfoChanged(item);
}

QnPeerRuntimeInfo QnRuntimeInfoManager::localInfo() const {
    Q_ASSERT(m_items->hasItem(qnCommon->moduleGUID()));
    return m_items->getItem(qnCommon->moduleGUID());
}
