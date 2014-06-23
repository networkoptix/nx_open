#include "runtime_info_manager.h"
#include "transaction/transaction.h"
#include "api/common_message_processor.h"
#include "transaction/transaction_message_bus.h"

namespace ec2
{

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
    m_runtimeInfo.insert(runtimeInfo.peer.id, runtimeInfo);
    qnLicensePool->addRemoteValidLicenses(runtimeInfo.peer.id, runtimeInfo.validLicenses);
}

void QnRuntimeInfoManager::update(const ec2::ApiRuntimeData& value)
{
    if (m_runtimeInfo.value(value.peer.id) == value)
        return;

    m_runtimeInfo.insert(value.peer.id, value);

    QnTransaction<ApiRuntimeData> tran(ApiCommand::runtimeInfoChanged, false);
    tran.params = value;
    tran.fillSequence();
    qnTransactionBus->sendTransaction(tran);
}

QMap<QnId, ec2::ApiRuntimeData> QnRuntimeInfoManager::data()
{
    return m_runtimeInfo;
}

QnRuntimeInfoManager* QnRuntimeInfoManager::instance()
{
    return m_inst;
}

}
