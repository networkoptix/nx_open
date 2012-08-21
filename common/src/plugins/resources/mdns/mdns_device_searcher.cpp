#include "mdns_device_searcher.h"
#include "utils/network/nettools.h"
#include "utils/network/mdns.h"
#include "utils/common/sleep.h"
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
        QString removeAddress = data[i].removeAddress;

        QList<QnNetworkResourcePtr> nresourceLst = processPacket(result, data[i].response, QHostAddress(localAddress));

        foreach(QnNetworkResourcePtr nresource, nresourceLst)
        {
            nresource->setHostAddress(QHostAddress(removeAddress), QnDomainMemory);
            nresource->setDiscoveryAddr(QHostAddress(localAddress));
            result.push_back(nresource);
        }

    }

    return result;
}

int strEqualAmount(const char* str1, const char* str2)
{
    int rez = 0;
    while (*str1 && *str1 == *str2)
    {
        rez++;
        str1++;
        str2++;
    }
    return rez;
}

bool QnMdnsResourceSearcher::isNewDiscoveryAddressBetter(const QString& host, const QString& newAddress, const QString& oldAddress)
{
    int eq1 = strEqualAmount(host.toLocal8Bit().constData(), newAddress.toLocal8Bit().constData());
    int eq2 = strEqualAmount(host.toLocal8Bit().constData(), oldAddress.toLocal8Bit().constData());
    return eq1 > eq2;
}
