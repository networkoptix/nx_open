#include "iqinvision_resource_searcher.h"
#if defined(ENABLE_IQE)

#include "core/resource/camera_resource.h"
#include "iqinvision_resource.h"
#include "utils/common/sleep.h"
#include "core/resource/resource_data.h"
#include "core/resource_management/resource_data_pool.h"
#include "common/common_module.h"
#include <common/static_common_module.h>

#if !defined(Q_OS_WIN)
    #include <arpa/inet.h>
#endif

namespace {

static constexpr quint16 kNativeDiscoveryRequestPort = 43282;
static constexpr quint16 kNativeDiscoveryResponsePort = 43283;

static constexpr int kRequestSize = 8;
static const char* const requests[] = {
    "\x01\x01\x00\x00\x00\x3f\x00\x00",
    "\x01\x01\x00\x00\x40\x7f\x00\x00",
    "\x01\x01\x00\x00\x80\xbf\x00\x00",
    "\x01\x01\x00\x00\xc0\xff\x00\x00"
};

static const QString kDefaultResourceType(lit("IQA32N"));

} // namespace

QnPlIqResourceSearcher::QnPlIqResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule),
    QnAbstractNetworkResourceSearcher(commonModule),
    QnMdnsResourceSearcher(commonModule)
{
}

QnResourcePtr QnPlIqResourceSearcher::createResource(
    const QnUuid& resourceTypeId, const QnResourceParams& /*params*/)
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

    result = QnVirtualCameraResourcePtr(new QnPlIqResource());
    result->setTypeId(resourceTypeId);

    qDebug() << "Create IQE camera resource. typeID:" << resourceTypeId.toString();
    // << ", Parameters: " << parameters;

    //result->deserialize(parameters);

    return result;
}

QString QnPlIqResourceSearcher::manufacture() const
{
    return QnPlIqResource::MANUFACTURE;
}

bool QnPlIqResourceSearcher::isIqeModel(const QString& model)
{
    static const QRegExp kCameraModelPattern("^IQ(eye)?[A-Z0-9]?[0-9]{2,4}[A-Z]{0,2}$");
    return kCameraModelPattern.exactMatch(model);
}

QList<QnResourcePtr> QnPlIqResourceSearcher::checkHostAddr(
    const nx::utils::Url& url, const QAuthenticator& /*auth*/, bool isSearchAction)
{
    if (!url.scheme().isEmpty() && isSearchAction)
        return QList<QnResourcePtr>(); //< Search if only host is present, not specific protocol.

    return QList<QnResourcePtr>();
}

QList<QnNetworkResourcePtr> QnPlIqResourceSearcher::processPacket(
    QnResourceList& result,
    const QByteArray& responseData,
    const QHostAddress& /*discoveryAddress*/,
    const QHostAddress& /*foundHostAddress*/)
{
    QString smac;
    QString name;

    QList<QnNetworkResourcePtr> localResults;

    const int iqpos = responseData.indexOf("IQ");
    if (iqpos < 0)
        return localResults;

    int macpos = responseData.indexOf("00", iqpos);
    if (macpos < 0)
        return localResults;

    for (int i = iqpos; i < macpos; i++)
        name += QLatin1Char(responseData[i]);

    name.replace(QLatin1Char(' '), QString());
    name.replace(QLatin1Char('-'), QString());
    name.replace(QLatin1Char('\t'), QString());

    if (!isIqeModel(name.trimmed()))
        return localResults; //< not an IQ camera

    if (macpos + 12 > responseData.size())
        return localResults;

    while (responseData.at(macpos) == ' ' && macpos < responseData.size())
        ++macpos;

    if (macpos + 12 > responseData.size())
        return localResults;

    for (int i = 0; i < 12; ++i)
    {
        if (i > 0 && i % 2 == 0)
            smac += QLatin1Char('-');
        smac += QLatin1Char(responseData[macpos + i]);
    }

    //response.fromDatagram(responseData);

    smac = smac.toUpper();
    QnMacAddress macAddress(smac);
    if (macAddress.isNull())
        return localResults;

    for (const QnResourcePtr& res: result)
    {
        const QnNetworkResourcePtr netRes = res.dynamicCast<QnNetworkResource>();
        if (netRes->getMAC().toString() == smac)
            return localResults; //< Already found.
    }

    QnResourceData resourceData = qnStaticCommon->dataPool()->data(manufacture(), name);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return localResults; //< Model forced by ONVIF.

    QnPlIqResourcePtr resource ( new QnPlIqResource() );

    QnUuid rt = qnResTypePool->getResourceTypeId(manufacture(), name, false);
    if (rt.isNull())
    {
        // Try with default camera name.
        rt = qnResTypePool->getResourceTypeId(manufacture(), kDefaultResourceType);
        if (rt.isNull())
            return localResults;
    }

    resource->setTypeId(rt);
    resource->setName(name);
    resource->setModel(name);
    resource->setMAC(macAddress);

    localResults.push_back(resource);

    return localResults;
}

void QnPlIqResourceSearcher::processNativePacket(
    QnResourceList& result, const QByteArray& responseData)
{
#if 0 // debug
    QFile gggFile("c:/123");
    gggFile.open(QFile::ReadOnly);
    responseData = gggFile.readAll();
#endif // 0

    if (responseData.at(0) != 0x01 || responseData.at(1) != 0x04 || responseData.at(2) != 0x00 ||
        responseData.at(3) != 0x00 || responseData.at(4) != 0x00 || responseData.at(5) != 0x00)
    {
        return;
    }

    QnMacAddress macAddr;
    for (int i = 0; i < 6; ++i)
        macAddr.setByte(i, responseData.at(i+6));

    int iqpos = responseData.indexOf("IQ"); //< name
    iqpos = responseData.indexOf("IQ", iqpos + 2); //< vendor
    iqpos = responseData.indexOf("IQ", iqpos + 2); //< type
    if (iqpos < 0)
        return;

    QByteArray name(responseData.data() + iqpos); //< Construct from null-terminated string.

    for (const QnResourcePtr& res: result)
    {
        const QnNetworkResourcePtr netRes = res.dynamicCast<QnNetworkResource>();
        if (netRes->getMAC() == macAddr)
            return; // Already found.
    }

    const QString nameStr = QString::fromLatin1(name);
    QnUuid rt = qnResTypePool->getResourceTypeId(manufacture(), nameStr, /*showWarning*/ false);
    if (rt.isNull())
    {
        rt = qnResTypePool->getResourceTypeId(manufacture(), kDefaultResourceType);
        if (rt.isNull())
            return;
    }

    QnPlIqResourcePtr resource (new QnPlIqResource());
    in_addr* peerAddr = (in_addr*) (responseData.data() + 32);
    QHostAddress peerAddress(QLatin1String(inet_ntoa(*peerAddr)));
    resource->setTypeId(rt);
    resource->setName(nameStr);
    resource->setModel(nameStr);
    resource->setMAC(macAddr);
    resource->setHostAddress(peerAddress.toString());

    result.push_back(resource);
}

QnResourceList QnPlIqResourceSearcher::findResources()
{
    QnResourceList result = QnMdnsResourceSearcher::findResources();

    std::unique_ptr<AbstractDatagramSocket> receiveSock(SocketFactory::createDatagramSocket());
    if (!receiveSock->bind(SocketAddress( HostAddress::anyHost, kNativeDiscoveryResponsePort)))
        return result;

    for (const QnInterfaceAndAddr& iface: getAllIPv4Interfaces())
    {
        std::unique_ptr<AbstractDatagramSocket> sendSock(SocketFactory::createDatagramSocket());
        if (!sendSock->bind(iface.address.toString(), kNativeDiscoveryRequestPort))
            continue;

        for (size_t i = 0; i < sizeof(requests) / sizeof(char*); ++i)
        {
            // Sending broadcast.
            QByteArray datagram(requests[i], kRequestSize);
            sendSock->sendTo(
                datagram.data(), datagram.size(), BROADCAST_ADDRESS, kNativeDiscoveryRequestPort);
        }
    }

    QnSleep::msleep(300);

    while (receiveSock->hasData())
    {
        QByteArray datagram;
        datagram.resize(AbstractDatagramSocket::MAX_DATAGRAM_SIZE);

        SocketAddress senderEndpoint;
        int bytesRead = receiveSock->recvFrom(datagram.data(), datagram.size(), &senderEndpoint);

        static constexpr int kMinResponseSize = 128;
        if (senderEndpoint.port == kNativeDiscoveryResponsePort && bytesRead > kMinResponseSize)
            processNativePacket(result, datagram.left(bytesRead));
    }

    return result;
}

#endif // defined(ENABLE_IQE)
