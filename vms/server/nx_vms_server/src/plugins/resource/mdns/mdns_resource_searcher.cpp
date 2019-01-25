#include "mdns_resource_searcher.h"

#ifdef ENABLE_MDNS

#include <nx/network/nettools.h>

#include "mdns_listener.h"
#include <media_server/media_server_module.h>

QnMdnsResourceSearcher::QnMdnsResourceSearcher(QnMediaServerModule* serverModule):
    QnAbstractResourceSearcher(serverModule->commonModule()),
    QnAbstractNetworkResourceSearcher(serverModule->commonModule()),
    m_serverModule(serverModule)
{
    serverModule->mdnsListener()->registerConsumer((std::uintptr_t) this);
}

QnMdnsResourceSearcher::~QnMdnsResourceSearcher()
{

}

//bool QnMdnsResourceSearcher::isProxy() const
//{
//    return false;
//}

QnResourceList QnMdnsResourceSearcher::findResources()
{
    QnResourceList result;

    auto consumerData = m_serverModule->mdnsListener()->getData((std::uintptr_t) this);
    consumerData->forEachEntry(
        [this, &result](
            const QString& remoteAddress,
            const QString& localAddress,
            const QByteArray& responseData)
        {
            const QList<QnNetworkResourcePtr>& nresourceLst = processPacket(
                result,
                responseData,
                QHostAddress(localAddress),
                QHostAddress(remoteAddress));

            for(const QnNetworkResourcePtr& nresource: nresourceLst)
            {
                nresource->setHostAddress(remoteAddress);
                result.push_back(nresource);
            }
        });

    return result;
}

#endif // ENABLE_MDNS
