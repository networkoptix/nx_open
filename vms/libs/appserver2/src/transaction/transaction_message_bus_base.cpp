// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "transaction_message_bus_base.h"

#include <common/common_module.h>
#include <nx/vms/ec2/ec_connection_notification_manager.h>
#include <utils/common/waiting_for_qthread_to_empty_event_queue.h>

using namespace nx::vms;

namespace ec2 {

TransactionMessageBusBase::TransactionMessageBusBase(
    api::PeerType peerType,
    nx::vms::common::SystemContext* systemContext,
    QnJsonTransactionSerializer* jsonTranSerializer,
    QnUbjsonTransactionSerializer* ubjsonTranSerializer)
    :
    AbstractTransactionMessageBus(systemContext),
    m_thread(new QThread()),
    m_jsonTranSerializer(jsonTranSerializer),
    m_ubjsonTranSerializer(ubjsonTranSerializer),
    m_localPeerType(peerType),
    m_mutex(nx::Mutex::Recursive)
{
    qRegisterMetaType<api::PeerType>();
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
    NX_VERBOSE(this, "Before stop");
    if (m_thread->isRunning())
    {
        m_thread->exit();
        m_thread->wait();
    }
    NX_VERBOSE(this, "After stop");
}

void TransactionMessageBusBase::setHandler(ECConnectionNotificationManager* handler)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    NX_ASSERT(!m_thread->isRunning());
    NX_ASSERT(m_handler == NULL, "Previous handler must be removed at this time");
    m_handler = handler;
}

void TransactionMessageBusBase::removeHandler(ECConnectionNotificationManager* handler)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    NX_ASSERT(!m_thread->isRunning());
    if (m_handler)
    {
        NX_ASSERT(m_handler == handler, "We must remove only current handler");
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

} // namespace ec2
