
#include "transaction_transport.h"

#include "core/resource/media_server_resource.h"
#include "api/app_server_connection.h"
#include "database/db_manager.h"
#include <core/resource_management/resource_pool.h>


namespace ec2 {

QnTransactionTransport::QnTransactionTransport(
    const QnUuid& connectionGuid,
    const ApiPeerData& localPeer,
    const ApiPeerData& remotePeer,
    QSharedPointer<AbstractStreamSocket> socket,
    ConnectionType::Type connectionType,
    const nx_http::Request& request,
    const QByteArray& contentEncoding,
    const Qn::UserAccessData &userAccessData)
:
    QnTransactionTransportBase(
        connectionGuid,
        localPeer,
        remotePeer,
        std::move(socket),
        connectionType,
        request,
        contentEncoding,
        userAccessData)
{
}

QnTransactionTransport::QnTransactionTransport(const ApiPeerData& localPeer)
:
    QnTransactionTransportBase(localPeer)
{
}

void QnTransactionTransport::fillAuthInfo(const nx_http::AsyncHttpClientPtr& httpClient, bool authByKey)
{
    if (!QnAppServerConnectionFactory::videowallGuid().isNull())
    {
        httpClient->addAdditionalHeader(
            "X-NetworkOptix-VideoWall",
            QnAppServerConnectionFactory::videowallGuid().toString().toUtf8());
        return;
    }

    QnMediaServerResourcePtr ownServer = 
        qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    if (ownServer && authByKey)
    {
        httpClient->setUserName(ownServer->getId().toString().toLower());
        httpClient->setUserPassword(ownServer->getAuthKey());
    }
    else
    {
        QUrl url = QnAppServerConnectionFactory::url();
        httpClient->setUserName(url.userName().toLower());
        if (detail::QnDbManager::instance() && detail::QnDbManager::instance()->isInitialized())
        {
            // try auth by admin user if allowed
            QnUserResourcePtr adminUser = qnResPool->getAdministrator();
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

}   // namespace ec2
