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

/*
void QnRuntimeInfoManager::update(const ec2::ApiRuntimeData& value)
{
    Q_ASSERT(!value.peer.id.isNull() && value.peer.peerType == Qn::PT_Server);

    QMutexLocker lock(&m_mutex);

    ec2::ApiRuntimeData existingData = m_runtimeInfo.value(value.peer.id);
    if (existingData == value)
        return;

    ec2::ApiRuntimeData newData = value;
    newData.version = existingData.version + 1;
    m_runtimeInfo.insert(value.peer.id, newData);
    emit runtimeInfoChanged(newData);
}*/

/*
ec2::ApiRuntimeData QnRuntimeInfoManager::data(const QnId& id) const {
    if (!hasData(id)) {
        ec2::ApiRuntimeData result;
        result.peer.id = id;
        return result;
    }

    QMutexLocker lock(&m_mutex);
    return m_runtimeInfo.value(id);
}*/

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

    //clear all info on disconnect
    m_items->setItems(QnPeerRuntimeInfoMap());
}

void QnRuntimeInfoManager::storedItemChanged(const QnPeerRuntimeInfo &item) {
    emit runtimeInfoChanged(item);
}
