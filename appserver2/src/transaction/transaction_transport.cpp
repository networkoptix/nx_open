#include "transaction_transport.h"

#include <common/common_module.h>

#include <api/app_server_connection.h>
#include <api/global_settings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include "database/db_manager.h"


namespace ec2 {

QnTransactionTransport::QnTransactionTransport(
    QnCommonModule* commonModule,
    const QnUuid& connectionGuid,
    ConnectionLockGuard connectionLockGuard,
    const ApiPeerData& localPeer,
    const ApiPeerData& remotePeer,
    QSharedPointer<AbstractStreamSocket> socket,
    ConnectionType::Type connectionType,
    const nx_http::Request& request,
    const QByteArray& contentEncoding,
    const Qn::UserAccessData &userAccessData)
:
    QnTransactionTransportBase(
        commonModule->globalSettings()->localSystemId(),
        connectionGuid,
        std::move(connectionLockGuard),
        localPeer,
        remotePeer,
        connectionType,
        request,
        contentEncoding,
        commonModule->globalSettings()->connectionKeepAliveTimeout(),
        commonModule->globalSettings()->keepAliveProbeCount()),
    QnCommonModuleAware(commonModule),
    m_userAccessData(userAccessData)
{
    setOutgoingConnection(std::move(socket));
}

QnTransactionTransport::QnTransactionTransport(
    QnCommonModule* commonModule,
    ConnectionGuardSharedState* const connectionGuardSharedState,
    const ApiPeerData& localPeer)
:
    QnTransactionTransportBase(
        commonModule->globalSettings()->localSystemId(),
        connectionGuardSharedState,
        localPeer,
        commonModule->globalSettings()->connectionKeepAliveTimeout(),
        commonModule->globalSettings()->keepAliveProbeCount()),
    QnCommonModuleAware(commonModule),
    m_userAccessData(Qn::kSystemAccess)
{
}

QnTransactionTransport::~QnTransactionTransport()
{
    pleaseStopSync();

    if (m_beforeDestructionHandler)
        m_beforeDestructionHandler();
}

void QnTransactionTransport::close()
{
    // Changing state before freeing socket so that everyone
    // stop using socket before it is actually freed.
    setState(State::Closed);

    pleaseStopSync();

    markAsNotSynchronized();
}

void QnTransactionTransport::fillAuthInfo(const nx_http::AsyncHttpClientPtr& httpClient, bool authByKey)
{
    if (!commonModule()->videowallGuid().isNull())
    {
        httpClient->addAdditionalHeader(
            "X-NetworkOptix-VideoWall",
            commonModule()->videowallGuid().toString().toUtf8());
        return;
    }

    const auto& resPool = commonModule()->resourcePool();
    QnMediaServerResourcePtr ownServer =
        resPool->getResourceById<QnMediaServerResource>(localPeer().id);
    if (ownServer && authByKey)
    {
        httpClient->setUserName(ownServer->getId().toString().toLower());
        httpClient->setUserPassword(ownServer->getAuthKey());
    }
    else
    {
        QUrl url;
        if (const auto& connection = commonModule()->ec2Connection())
            url = connection->connectionInfo().ecUrl;
        httpClient->setUserName(url.userName().toLower());
        if (ApiPeerData::isServer(localPeer().peerType))
        {
            // try auth by admin user if allowed
            QnUserResourcePtr adminUser = resPool->getAdministrator();
            if (adminUser)
            {
                httpClient->setUserPassword(adminUser->getDigest());
                httpClient->setAuthType(nx_http::AsyncHttpClient::authDigestWithPasswordHash);
            }
        }
        else
        {
            httpClient->setUserPassword(url.password());
        }
    }
}

bool QnTransactionTransport::sendSerializedTransaction(
    Qn::SerializationFormat srcFormat,
    const QByteArray& serializedTran,
    const QnTransactionTransportHeader& _header)
{
    if (srcFormat != remotePeer().dataFormat)
        return false;

    /* Check if remote peer has rights to receive transaction */
    if (m_userAccessData.userId != Qn::kSystemAccess.userId)
    {
        NX_LOG(
            QnLog::EC2_TRAN_LOG,
            lit("Permission check failed while sending SERIALIZED transaction to peer %1")
            .arg(remotePeer().id.toString()),
            cl_logDEBUG1);
        return false;
    }

    QnTransactionTransportHeader header(_header);
    NX_ASSERT(header.processedPeers.contains(localPeer().id));
    header.fillSequence(commonModule()->moduleGUID(), commonModule()->runningInstanceGUID());
    switch (remotePeer().dataFormat)
    {
        case Qn::JsonFormat:
            addData(QnJsonTransactionSerializer::instance()->serializedTransactionWithoutHeader(serializedTran, header) + QByteArray("\r\n"));
            break;
            //case Qn::BnsFormat:
            //    addData(QnBinaryTransactionSerializer::instance()->serializedTransactionWithHeader(serializedTran, header));
            break;
        case Qn::UbjsonFormat: {

            if (nx::utils::log::isToBeLogged(nx::utils::log::Level::debug, QnLog::EC2_TRAN_LOG))
            {
                QnAbstractTransaction abtractTran;
                QnUbjsonReader<QByteArray> stream(&serializedTran);
                QnUbjson::deserialize(&stream, &abtractTran);
                NX_LOG(QnLog::EC2_TRAN_LOG, lit("send direct transaction %1 to peer %2").arg(abtractTran.toString()).arg(remotePeer().id.toString()), cl_logDEBUG1);
            }

            addData(QnUbjsonTransactionSerializer::instance()->serializedTransactionWithHeader(serializedTran, header));
            break;
        }
        default:
            qWarning() << "Client has requested data in the unsupported format" << remotePeer().dataFormat;
            addData(QnUbjsonTransactionSerializer::instance()->serializedTransactionWithHeader(serializedTran, header));
            break;
    }

    return true;
}

void QnTransactionTransport::setBeforeDestroyCallback(
    std::function<void()> ttFinishCallback)
{
    m_beforeDestructionHandler = ttFinishCallback;
}

}   // namespace ec2
