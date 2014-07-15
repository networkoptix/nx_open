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
    connect( QnCommonMessageProcessor::instance(), &QnCommonMessageProcessor::connectionClosed,   this, &QnRuntimeInfoManager::at_connectionClosed );
}

QnRuntimeInfoManager::~QnRuntimeInfoManager()
{
    m_inst = 0;
}

QnRuntimeInfoManager* instance()
{
    return m_inst;
}

void QnRuntimeInfoManager::at_remotePeerFound(const ec2::ApiPeerAliveData &, bool)
{
}

void QnRuntimeInfoManager::at_remotePeerLost(const ec2::ApiPeerAliveData &data, bool)
{
    m_runtimeInfo.remove(data.peer.id);

    if (data.peer.id == qnCommon->remoteGUID()) {
        m_runtimeInfo.clear();
    }
}

void QnRuntimeInfoManager::at_connectionClosed()
{
    m_runtimeInfo.clear();
}

void QnRuntimeInfoManager::at_runtimeInfoChanged(const ec2::ApiRuntimeData &runtimeInfo)
{
    QMutexLocker lock(&m_mutex);

    ec2::ApiRuntimeData existingData = m_runtimeInfo.value(runtimeInfo.peer.id);
    if (existingData.version >= runtimeInfo.version)
        return;

    m_runtimeInfo.insert(runtimeInfo.peer.id, runtimeInfo);

    lock.unlock();  //to avoid dead-lock in case if directly (or auto) connected slot calls any method of this class
    emit runtimeInfoChanged(runtimeInfo);
}

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
    lock.unlock();
    emit runtimeInfoChanged(newData);
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
