#ifdef ENABLE_IQE

#include "core/resource/camera_resource.h"
#include "iqinvision_resource_searcher.h"
#include "iqinvision_resource.h"
#include "utils/common/sleep.h"

#ifndef Q_OS_WIN
#include <arpa/inet.h>
#endif

static quint16 NATIVE_DISCOVERY_REQUEST_PORT = 43282;
static quint16 NATIVE_DISCOVERY_RESPONSE_PORT = 43283;

static const int REQUEST_SIZE = 8;
static const char* requests[] =
{
    "\x01\x01\x00\x00\x00\x3f\x00\x00",
    "\x01\x01\x00\x00\x40\x7f\x00\x00",
    "\x01\x01\x00\x00\x80\xbf\x00\x00",
    "\x01\x01\x00\x00\xc0\xff\x00\x00"
};


QnPlIqResourceSearcher::QnPlIqResourceSearcher()
{
}

QnResourcePtr QnPlIqResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& /*params*/)
{
    QnNetworkResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
    {
        qDebug() << "No resource type for ID = " << resourceTypeId;

        return result;
    }

    if (resourceType->getManufacture() != manufacture())
    {
        //qDebug() << "Manufature " << resourceType->getManufacture() << " != " << manufacture();
        return result;
    }

    result = QnVirtualCameraResourcePtr( new QnPlIqResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create IQE camera resource. typeID:" << resourceTypeId.toString(); // << ", Parameters: " << parameters;

    //result->deserialize(parameters);

    return result;

}

QString QnPlIqResourceSearcher::manufacture() const
{
    return QnPlIqResource::MANUFACTURE;
}


QList<QnResourcePtr> QnPlIqResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool isSearchAction)
{
    if( !url.scheme().isEmpty() && isSearchAction )
        return QList<QnResourcePtr>();  //searching if only host is present, not specific protocol

    Q_UNUSED(url)
    Q_UNUSED(auth)
    return QList<QnResourcePtr>();
}

QList<QnNetworkResourcePtr> QnPlIqResourceSearcher::processPacket(
    QnResourceList& result,
    const QByteArray& responseData,
    const QHostAddress& discoveryAddress,
    const QHostAddress& /*foundHostAddress*/ )
{

    QString smac;
    QString name;

    QList<QnNetworkResourcePtr> local_results;

    int iqpos = responseData.indexOf("IQ");


    if (iqpos<0)
        return local_results;

    int macpos = responseData.indexOf("00", iqpos);
    if (macpos < 0)
        return local_results;

    for (int i = iqpos; i < macpos; i++)
    {
        name += QLatin1Char(responseData[i]);
    }

    name.replace(QLatin1Char(' '), QString()); // remove spaces
    name.replace(QLatin1Char('-'), QString()); // remove spaces
    name.replace(QLatin1Char('\t'), QString()); // remove tabs

    if (macpos+12 > responseData.size())
        return local_results;


    //macpos++; // -

    while(responseData.at(macpos)==' ')
        ++macpos;


    if (macpos+12 > responseData.size())
        return local_results;



    for (int i = 0; i < 12; i++)
    {
        if (i > 0 && i % 2 == 0)
            smac += QLatin1Char('-');

        smac += QLatin1Char(responseData[macpos + i]);
    }


    //response.fromDatagram(responseData);

    smac = smac.toUpper();

    foreach(QnResourcePtr res, result)
    {
        QnNetworkResourcePtr net_res = res.dynamicCast<QnNetworkResource>();
    
        if (net_res->getMAC().toString() == smac)
        {
            if (isNewDiscoveryAddressBetter(net_res->getHostAddress(), discoveryAddress.toString(), net_res->getDiscoveryAddr().toString()))
                net_res->setDiscoveryAddr(discoveryAddress);
            return local_results; // already found;
        }
    }


    QnPlIqResourcePtr resource ( new QnPlIqResource() );

    QnUuid rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (rt.isNull())
    {
        // try with default camera name
        name = QLatin1String("IQA32N");
        rt = qnResTypePool->getResourceTypeId(manufacture(), name);

        if (rt.isNull())
            return local_results;
    }

    resource->setTypeId(rt);
    resource->setName(name);
    resource->setModel(name);
    resource->setMAC(QnMacAddress(smac));

    local_results.push_back(resource);

    return local_results;
}

void QnPlIqResourceSearcher::processNativePacket(QnResourceList& result, const QByteArray& responseData, const QHostAddress& discoveryAddress)
{
    /*
    QFile gggFile("c:/123");
    gggFile.open(QFile::ReadOnly);
    responseData = gggFile.readAll();
    */

    if (responseData.at(0) != 0x01 || responseData.at(1) != 0x04 || responseData.at(2) != 0x00 || 
        responseData.at(3) != 0x00 || responseData.at(4) != 0x00 || responseData.at(5) != 0x00)
    {
        return;
    }

    QnMacAddress macAddr;
    for (int i = 0; i < 6; i++)
        macAddr.setByte(i, responseData.at(i+6));

    int iqpos = responseData.indexOf("IQ"); // name
    iqpos = responseData.indexOf("IQ", iqpos+2); // vendor
    iqpos = responseData.indexOf("IQ", iqpos+2); // type
    if (iqpos < 0)
        return;

    QByteArray name(responseData.data() + iqpos); // construct from null terminated char*

    foreach(QnResourcePtr res, result)
    {
        QnNetworkResourcePtr net_res = res.dynamicCast<QnNetworkResource>();

        if (net_res->getMAC() == macAddr)
        {
            if (isNewDiscoveryAddressBetter(net_res->getHostAddress(), discoveryAddress.toString(), net_res->getDiscoveryAddr().toString()))
                net_res->setDiscoveryAddr(discoveryAddress);
            return; // already found;
        }
    }

    QString nameStr = QString::fromLatin1(name);
    QnUuid rt = qnResTypePool->getResourceTypeId(manufacture(), nameStr);
    if (rt.isNull()) {
        qWarning() << "Unregistered IQvision camera type:" << name;
        return;
    }

    QnPlIqResourcePtr resource ( new QnPlIqResource() );
    in_addr* peer_addr = (in_addr*) (responseData.data() + 32);
    QHostAddress peerAddress(QLatin1String(inet_ntoa(*peer_addr)));
    resource->setTypeId(rt);
    resource->setName(nameStr);
    resource->setModel(nameStr);
    resource->setMAC(macAddr);
    resource->setHostAddress(peerAddress.toString());
    resource->setDiscoveryAddr(discoveryAddress);

    result.push_back(resource);
}


QnResourceList QnPlIqResourceSearcher::findResources()
{
    QnResourceList result = QnMdnsResourceSearcher::findResources();

    foreach (QnInterfaceAndAddr iface, getAllIPv4Interfaces())
    {
        std::unique_ptr<AbstractDatagramSocket> sendSock( SocketFactory::createDatagramSocket() );
        std::unique_ptr<AbstractDatagramSocket> receiveSock( SocketFactory::createDatagramSocket() );
        if (!sendSock->bind(iface.address.toString(), NATIVE_DISCOVERY_REQUEST_PORT))
            continue;
        if (!receiveSock->bind(iface.address.toString(), NATIVE_DISCOVERY_RESPONSE_PORT))
            continue;

        for (uint i = 0; i < sizeof(requests)/sizeof(char*); ++i)
        {
            // sending broadcast
            QByteArray datagram(requests[i], REQUEST_SIZE);
            sendSock->sendTo(datagram.data(), datagram.size(), BROADCAST_ADDRESS, NATIVE_DISCOVERY_REQUEST_PORT);
        }

        QnSleep::msleep(300);

        while (receiveSock->hasData())
        {
            QByteArray datagram;
            datagram.resize( AbstractDatagramSocket::MAX_DATAGRAM_SIZE );

            QString sender;
            quint16 senderPort;

            int readed = receiveSock->recvFrom(datagram.data(), datagram.size(), sender, senderPort);

            if (senderPort == NATIVE_DISCOVERY_RESPONSE_PORT && readed > 128) // minimum response size
                processNativePacket(result, datagram.left(readed), iface.address);
        }
        //processNativePacket(result, QByteArray(), iface.address);
    }

    return result;

}

#endif // #ifdef ENABLE_IQE
