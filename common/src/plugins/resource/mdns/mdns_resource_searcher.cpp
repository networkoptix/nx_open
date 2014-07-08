#include "mdns_resource_searcher.h"

#ifdef ENABLE_MDNS

#include <utils/network/nettools.h>

#include "mdns_listener.h"

QnMdnsResourceSearcher::QnMdnsResourceSearcher()
{
    QnMdnsListener::instance()->registerConsumer((long) this);
}

QnMdnsResourceSearcher::~QnMdnsResourceSearcher()
{

}

bool QnMdnsResourceSearcher::isProxy() const
{
    return false;
}

QnResourceList QnMdnsResourceSearcher::findResources()
{
    QnResourceList result;

    QnMdnsListener::ConsumerDataList data = QnMdnsListener::instance()->getData((long) this);
    for (int i = 0; i < data.size(); ++i)
    {
        QString localAddress = data[i].localAddress;
        QString removeAddress = data[i].remoteAddress;

        QList<QnNetworkResourcePtr> nresourceLst = processPacket(result, data[i].response, QHostAddress(localAddress));

        foreach(QnNetworkResourcePtr nresource, nresourceLst)
        {
            nresource->setHostAddress(removeAddress, QnDomainMemory);
            nresource->setDiscoveryAddr(QHostAddress(localAddress));
            result.push_back(nresource);
        }

    }

    return result;
}

#endif // ENABLE_MDNS
