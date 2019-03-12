#pragma once

#include <QtCore/QObject>

#include <nx/utils/safe_direct_connection.h>
#include <nx/utils/uuid.h>

class QnRuntimeInfoManager;
struct QnPeerRuntimeInfo;

namespace ec2 {

class AbstractTransactionMessageBus;
class QnAbstractTransactionTransport;

/**
 * Registers connected clients by adding "RuntimeInfo" struct to the system's distributed DB.
 */
class ClientRegistrar:
    public QObject,
    public /*mixin*/ Qn::EnableSafeDirectConnection
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

    void loadQueryParams(
        QnPeerRuntimeInfo* peerRuntimeInfo,
        const std::multimap<QString, QString>& queryParams);

    bool loadQueryParam(
        const std::multimap<QString, QString>& queryParams,
        const QString& name,
        QnUuid* value);
};

} // namespace ec2
