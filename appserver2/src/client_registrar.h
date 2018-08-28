#pragma once

#include <QtCore/QObject>

#include <nx/utils/safe_direct_connection.h>
#include <nx/utils/uuid.h>

class QnRuntimeInfoManager;

namespace ec2 {

class AbstractTransactionMessageBus;
class QnAbstractTransactionTransport;

/**
 * Registers connected clients by adding "RuntimeInfo" struct to the system's distributed DB.
 */
class ClientRegistrar:
    public QObject,
    public Qn::EnableSafeDirectConnection
{
public:
    ClientRegistrar(
        AbstractTransactionMessageBus* messageBus,
        QnRuntimeInfoManager* runtimeInfoManager);
    virtual ~ClientRegistrar();

private:
    AbstractTransactionMessageBus* m_messageBus = nullptr;
    QnRuntimeInfoManager* m_runtimeInfoManager = nullptr;

    void onNewConnectionEstablished(QnAbstractTransactionTransport* transport);
};

} // namespace ec2
