#include "transaction_message_bus_base.h"
#include <common/common_module.h>
#include <utils/common/waiting_for_qthread_to_empty_event_queue.h>
#include <ec_connection_notification_manager.h>

namespace ec2 {

QnTransactionMessageBusBase::QnTransactionMessageBusBase(
    detail::QnDbManager* db,
    Qn::PeerType peerType,
    QnCommonModule* commonModule)
:
    QnCommonModuleAware(commonModule),
    m_db(db),
    m_thread(new QThread()),
    m_localPeerType(peerType),
    m_mutex(QnMutex::Recursive)
{
    moveToThread(m_thread);
}

QnTransactionMessageBusBase::~QnTransactionMessageBusBase()
{
    stop();
}

void QnTransactionMessageBusBase::start()
{
    NX_ASSERT(!m_thread->isRunning());
    if (!m_thread->isRunning())
        m_thread->start();
}

void QnTransactionMessageBusBase::stop()
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

void QnTransactionMessageBusBase::setHandler(ECConnectionNotificationManager* handler)
{
    QnMutexLocker lock(&m_mutex);
    NX_ASSERT(!m_thread->isRunning());
    NX_ASSERT(m_handler == NULL, Q_FUNC_INFO, "Previous handler must be removed at this time");
    m_handler = handler;
}

void QnTransactionMessageBusBase::removeHandler(ECConnectionNotificationManager* handler)
{
    QnMutexLocker lock(&m_mutex);
    NX_ASSERT(!m_thread->isRunning());
    NX_ASSERT(m_handler == handler, Q_FUNC_INFO, "We must remove only current handler");
    if (m_handler == handler)
        m_handler = nullptr;
}


} // namespace ec2
