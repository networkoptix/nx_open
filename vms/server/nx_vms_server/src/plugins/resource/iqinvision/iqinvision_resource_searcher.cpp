#include "iqinvision_resource_searcher.h"
#include "iqinvision_request_helper.h"
#include "iqinvision_common.h"

#include <nx/network/nettools.h>
#include "core/resource/camera_resource.h"
#include "iqinvision_resource.h"
#include "utils/common/sleep.h"
#include "core/resource/resource_data.h"
#include "core/resource_management/resource_data_pool.h"
#include "common/common_module.h"

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

QnPlIqResourceSearcher::QnPlIqResourceSearcher(QnMediaServerModule* serverModule):
    QnAbstractResourceSearcher(serverModule->commonModule()),
    QnAbstractNetworkResourceSearcher(serverModule->commonModule()),
    QnMdnsResourceSearcher(serverModule),
    m_serverModule(serverModule)
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

    result = QnVirtualCameraResourcePtr(new QnPlIqResource(m_serverModule));
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
    const nx::utils::Url& url, const QAuthenticator& auth, bool isSearchAction)
{
    if (!url.scheme().isEmpty() && isSearchAction)
        return QList<QnResourcePtr>(); //< Search if only host is present, not specific protocol.

    using namespace nx::vms::server;

    nx::utils::Url iqEyeUrl(url);
    iqEyeUrl.setScheme(QString::fromLatin1(nx::network::http::kUrlSchemeName));

    QnPlIqResourcePtr resource(new QnPlIqResource(m_serverModule));
    resource->setUrl(iqEyeUrl.toString());
    resource->setAuth(auth);

    auto helper = plugins::IqInvisionRequestHelper(resource);

    const auto manufacturerResponse = helper.oid(plugins::kIqInvisionOidManufacturer);
    if (!manufacturerResponse.isSuccessful())
        return QList<QnResourcePtr>();

    const auto vendor = manufacturerResponse.toString().trimmed();
    if (vendor != plugins::kIqInvisionManufacturer)
        return QList<QnResourcePtr>();

    resource->setVendor(manufacture());

    const auto macAddressResponse = helper.oid(plugins::kIqInvisionOidMacAddress);
    if (!macAddressResponse.isSuccessful())
        return QList<QnResourcePtr>();

    const nx::utils::MacAddress macAddress(macAddressResponse.toString().trimmed());
    if (macAddress.isNull())
        return QList<QnResourcePtr>();

    resource->setMAC(macAddress);

    auto modelResponse = helper.oid(plugins::kIqInvisionOidModel);
    if (!modelResponse.isSuccessful())
        return QList<QnResourcePtr>();

    const auto model = modelResponse.toString().trimmed();
    QnResourceData resourceData = dataPool()->data(manufacture(), model);
    if (resourceData.value<bool>(ResourceDataKey::kForceONVIF))
        return QList<QnResourcePtr>();

    const auto resourceTypeId = resourceType(model);
    if (resourceTypeId.isNull())
        return QList<QnResourcePtr>();

    resource->setModel(model);
    resource->setName(model);
    resource->setTypeId(resourceTypeId);

    const auto firmwareResponse = helper.oid(plugins::kIqInvisionOidFirmware);
    if (firmwareResponse.isSuccessful())
        resource->setFirmware(firmwareResponse.toString().trimmed());

    return {resource};
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
    nx::utils::MacAddress macAddress(smac);
    if (macAddress.isNull())
        return localResults;

    for (const QnResourcePtr& res: result)
    {
        const QnNetworkResourcePtr netRes = res.dynamicCast<QnNetworkResource>();
        if (netRes->getMAC().toString() == smac)
            return localResults; //< Already found.
    }

    QnResourceData resourceData = dataPool()->data(manufacture(), name);
    if (resourceData.value<bool>(ResourceDataKey::kForceONVIF))
        return localResults; //< Model forced by ONVIF.

    QnPlIqResourcePtr resource (new QnPlIqResource(m_serverModule));

    const auto rt = resourceType(name);
    if (rt.isNull())
        return localResults;

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
    if (responseData.at(0) != 0x01 || responseData.at(1) != 0x04 || responseData.at(2) != 0x00 ||
        responseData.at(3) != 0x00 || responseData.at(4) != 0x00 || responseData.at(5) != 0x00)
    {
        return;
    }

    static constexpr int kMacAddressOffset = 6;

    nx::utils::MacAddress::Data bytes;
    for (int i = 0; i < nx::utils::MacAddress::kMacAddressLength; ++i)
        bytes[i] = responseData.at(i + kMacAddressOffset);
    const nx::utils::MacAddress macAddr(bytes);

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
    const auto rt = resourceType(nameStr);
    if (rt.isNull())
        return;

    QnPlIqResourcePtr resource (new QnPlIqResource(m_serverModule));
    in_addr* peerAddr = (in_addr*) (responseData.data() + 32);
    QHostAddress peerAddress(QLatin1String(inet_ntoa(*peerAddr)));
    resource->setTypeId(rt);
    resource->setName(nameStr);
    resource->setModel(nameStr);
    resource->setMAC(macAddr);
    resource->setHostAddress(peerAddress.toString());

    result.push_back(resource);
}

QnUuid QnPlIqResourceSearcher::resourceType(const QString& model) const
{
    QnUuid resourceType = qnResTypePool->getResourceTypeId(manufacture(), model, false);
    if (!resourceType.isNull())
        return resourceType;

    // Try with default camera name.
    return qnResTypePool->getResourceTypeId(manufacture(), kDefaultResourceType);
}

QnResourceList QnPlIqResourceSearcher::findResources()
{
    QnResourceList result = QnMdnsResourceSearcher::findResources();

    std::unique_ptr<nx::network::AbstractDatagramSocket> receiveSock(nx::network::SocketFactory::createDatagramSocket());
    if (!receiveSock->bind(nx::network::SocketAddress( nx::network::HostAddress::anyHost, kNativeDiscoveryResponsePort)))
        return result;

    for (const nx::network::QnInterfaceAndAddr& iface: nx::network::getAllIPv4Interfaces())
    {
        std::unique_ptr<nx::network::AbstractDatagramSocket> sendSock(nx::network::SocketFactory::createDatagramSocket());
        if (!sendSock->bind(iface.address.toString(), kNativeDiscoveryRequestPort))
            continue;

        for (size_t i = 0; i < sizeof(requests) / sizeof(char*); ++i)
        {
            // Sending broadcast.
            QByteArray datagram(requests[i], kRequestSize);
            sendSock->sendTo(
                datagram.data(), datagram.size(), nx::network::BROADCAST_ADDRESS, kNativeDiscoveryRequestPort);
        }
    }

    QnSleep::msleep(300);

    while (receiveSock->hasData())
    {
        QByteArray datagram;
        datagram.resize(nx::network::AbstractDatagramSocket::MAX_DATAGRAM_SIZE);

        nx::network::SocketAddress senderEndpoint;
        int bytesRead = receiveSock->recvFrom(datagram.data(), datagram.size(), &senderEndpoint);

        static constexpr int kMinResponseSize = 128;
        if (senderEndpoint.port == kNativeDiscoveryResponsePort && bytesRead > kMinResponseSize)
            processNativePacket(result, datagram.left(bytesRead));
    }

    return result;
}
