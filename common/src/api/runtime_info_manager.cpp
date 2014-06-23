#include "runtime_info_manager.h"
#include "api/common_message_processor.h"
#include "api/app_server_connection.h"
#include "common/common_module.h"

static QnRuntimeInfoManager* m_inst;

QnRuntimeInfoManager::QnRuntimeInfoManager():
    QObject()
{
    Q_ASSERT(m_inst == 0);
    m_inst = this;

    connect( QnCommonMessageProcessor::instance(), &QnCommonMessageProcessor::runtimeInfoChanged, this, &QnRuntimeInfoManager::at_runtimeInfoChanged );
    connect( QnCommonMessageProcessor::instance(), &QnCommonMessageProcessor::remotePeerFound,    this, &QnRuntimeInfoManager::at_remotePeerFound );
    connect( QnCommonMessageProcessor::instance(), &QnCommonMessageProcessor::remotePeerLost,     this, &QnRuntimeInfoManager::at_remotePeerLost );
}

QnRuntimeInfoManager::~QnRuntimeInfoManager()
{
    m_inst = 0;
}

QnRuntimeInfoManager* instance()
{
    return m_inst;
}

void QnRuntimeInfoManager::at_remotePeerFound(ec2::ApiPeerAliveData data, bool isProxy)
{

}

void QnRuntimeInfoManager::at_remotePeerLost(ec2::ApiPeerAliveData data, bool isProxy)
{
    qnLicensePool->removeRemoteValidLicenses(data.peer.id);   //TODO: #Elric #ec2 get rid of the serialization hell
    m_runtimeInfo.remove(data.peer.id);
}

void QnRuntimeInfoManager::at_runtimeInfoChanged(const ec2::ApiRuntimeData& runtimeInfo)
{
    QMutexLocker lock(&m_mutex);

    m_runtimeInfo.insert(runtimeInfo.peer.id, runtimeInfo);

    QnId remoteID = qnCommon->remoteGUID();
    if (runtimeInfo.peer.id == remoteID) {
        qnLicensePool->setMainHardwareIds(runtimeInfo.mainHardwareIds);
        qnLicensePool->setCompatibleHardwareIds(runtimeInfo.compatibleHardwareIds);
    }
    else {
        qnLicensePool->addRemoteValidLicenses(runtimeInfo.peer.id, runtimeInfo.validLicenses);
    }
    emit runtimeInfoChanged(runtimeInfo);
}

void QnRuntimeInfoManager::update(const ec2::ApiRuntimeData& value)
{
    Q_ASSERT(!value.peer.id.isNull() && value.peer.peerType == Qn::PT_Server);

    QMutexLocker lock(&m_mutex);

    if (m_runtimeInfo.value(value.peer.id) == value)
        return;

    m_runtimeInfo.insert(value.peer.id, value);
    emit runtimeInfoChanged(value);
}

QMap<QnId, ec2::ApiRuntimeData> QnRuntimeInfoManager::allData() const
{
    QMutexLocker lock(&m_mutex);
    return m_runtimeInfo;
}

ec2::ApiRuntimeData QnRuntimeInfoManager::data(const QnId& id) const
{
    QMutexLocker lock(&m_mutex);
    if (m_runtimeInfo.contains(id)) {
        return m_runtimeInfo.value(id);
    }
    else {
        ec2::ApiRuntimeData result;
        result.peer.id = id;
        return result;
    }
}

QnRuntimeInfoManager* QnRuntimeInfoManager::instance()
{
    return m_inst;
}
