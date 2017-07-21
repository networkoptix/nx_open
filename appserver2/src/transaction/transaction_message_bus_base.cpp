#include "transaction_message_bus_base.h"
#include <common/common_module.h>
#include <utils/common/waiting_for_qthread_to_empty_event_queue.h>
#include <ec_connection_notification_manager.h>
#include <database/db_manager.h>

namespace ec2 {

TransactionMessageBusBase::TransactionMessageBusBase(
    detail::QnDbManager* db,
    Qn::PeerType peerType,
    QnCommonModule* commonModule,
    QnJsonTransactionSerializer* jsonTranSerializer,
    QnUbjsonTransactionSerializer* ubjsonTranSerializer)
:
    AbstractTransactionMessageBus(commonModule),
    m_db(db),
    m_thread(new QThread()),
    m_jsonTranSerializer(jsonTranSerializer),
    m_ubjsonTranSerializer(ubjsonTranSerializer),
    m_localPeerType(peerType),
    m_mutex(QnMutex::Recursive)
{
    qRegisterMetaType<Qn::PeerType>();
    moveToThread(m_thread);
}

TransactionMessageBusBase::~TransactionMessageBusBase()
{
    stop();
    delete m_thread;
}

void TransactionMessageBusBase::start()
{
    NX_ASSERT(!m_thread->isRunning());
    if (!m_thread->isRunning())
        m_thread->start();
}

void TransactionMessageBusBase::stop()
{
    if (m_thread->isRunning())
    {
        /* Connections in the 'Error' state will be closed via queued connection and after that removed via deleteLater() */
        WaitingForQThreadToEmptyEventQueue waitingForObjectsToBeFreed(m_thread, 7);
        waitingForObjectsToBeFreed.join();

        m_thread->exit();
        m_thread->wait();
    }
}

void TransactionMessageBusBase::setHandler(ECConnectionNotificationManager* handler)
{
    QnMutexLocker lock(&m_mutex);
    NX_ASSERT(!m_thread->isRunning());
    NX_ASSERT(m_handler == NULL, Q_FUNC_INFO, "Previous handler must be removed at this time");
    m_handler = handler;
}

void TransactionMessageBusBase::removeHandler(ECConnectionNotificationManager* handler)
{
    QnMutexLocker lock(&m_mutex);
    NX_ASSERT(!m_thread->isRunning());
    if (m_handler)
    {
        NX_ASSERT(m_handler == handler, Q_FUNC_INFO, "We must remove only current handler");
        if (m_handler == handler)
            m_handler = nullptr;
    }
}

QnJsonTransactionSerializer* TransactionMessageBusBase::jsonTranSerializer() const
{
    return m_jsonTranSerializer;
}

QnUbjsonTransactionSerializer* TransactionMessageBusBase::ubjsonTranSerializer() const
{
    return m_ubjsonTranSerializer;
}

ConnectionGuardSharedState* TransactionMessageBusBase::connectionGuardSharedState()
{
    return &m_connectionGuardSharedState;
}

void TransactionMessageBusBase::setTimeSyncManager(TimeSynchronizationManager* timeSyncManager)
{
    m_timeSyncManager = timeSyncManager;
}


} // namespace ec2
