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

int QnMdnsResourceSearcher::getSimilarity(const QString& remoteAddress, const QString& localAddress)
{
    const auto remoteParts = remoteAddress.splitRef('.');
    const auto localParts =localAddress.splitRef('.');
    int result = 0;
    for (int i = 0; i < localParts.size(); ++i)
    {
        if (remoteParts.size() < i || remoteParts[i] != localParts[i])
            break;
        ++result;
    }
    return result;
}

QnResourceList QnMdnsResourceSearcher::findResources()
{
    struct ResourceData
    {
        QnResourcePtr resource;
        int similarity = -1;
    };
    QMap<QnUuid, ResourceData> tmpResult;

    auto consumerData = m_serverModule->mdnsListener()->getData((std::uintptr_t) this);
    consumerData->forEachEntry(
        [this, &tmpResult](
            const QString& remoteAddress,
            const QString& localAddress,
            const QByteArray& responseData)
        {
            const QList<QnNetworkResourcePtr>& nresourceLst = processPacket(
                responseData,
                QHostAddress(localAddress),
                QHostAddress(remoteAddress));

            for(const QnNetworkResourcePtr& newResource: nresourceLst)
            {
                newResource->setHostAddress(remoteAddress);
                const int similarity = getSimilarity(remoteAddress, localAddress);
                if (similarity > tmpResult.value(newResource->getId()).similarity)
                    tmpResult.insert(newResource->getId(), ResourceData{newResource, similarity});
            }
        });

    QnResourceList result;
    for (const auto& data: tmpResult.values())
        result << data.resource;
    return result;
}

#endif // ENABLE_MDNS
