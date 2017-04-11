#include "mdns_resource_searcher.h"

#ifdef ENABLE_MDNS

#include <nx/network/nettools.h>

#include "mdns_listener.h"

QnMdnsResourceSearcher::QnMdnsResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule),
    QnAbstractNetworkResourceSearcher(commonModule)
{
    QnMdnsListener::instance()->registerConsumer((std::uintptr_t) this);
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

    QnMdnsListener::ConsumerDataList data = QnMdnsListener::instance()->getData((std::uintptr_t) this);
    for (int i = 0; i < data.size(); ++i)
    {
        const QString& localAddress = data[i].localAddress;
        const QString& remoteAddress = data[i].remoteAddress;

        const QList<QnNetworkResourcePtr>& nresourceLst = processPacket(
            result,
            data[i].response,
            QHostAddress(localAddress),
            QHostAddress(remoteAddress) );

        for(const QnNetworkResourcePtr& nresource: nresourceLst)
        {
            nresource->setHostAddress(remoteAddress);
            result.push_back(nresource);
        }

    }

    return result;
}

#endif // ENABLE_MDNS
