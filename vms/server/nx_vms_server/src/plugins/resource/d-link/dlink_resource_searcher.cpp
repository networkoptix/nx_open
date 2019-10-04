#ifdef ENABLE_DLINK

#include "dlink_resource_searcher.h"
#include "core/resource/camera_resource.h"
#include "dlink_resource.h"
#include <nx/network/nettools.h>
#include "utils/common/sleep.h"
#include <nx/network/socket.h>
#include "core/resource/resource_data.h"
#include "core/resource_management/resource_data_pool.h"
#include "common/common_module.h"

#include <nx/network/url/url_builder.h>

unsigned char request[] = {0xfd, 0xfd, 0x06, 0x00, 0xa1, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
QByteArray barequest(reinterpret_cast<char *>(request), sizeof(request));

char DCS[] = {'D', 'C', 'S', '-'};

extern QString getValueFromString(const QString& line);

#define CL_BROAD_CAST_RETRY 1

QnPlDlinkResourceSearcher::QnPlDlinkResourceSearcher(QnMediaServerModule* serverModule):
    QnAbstractResourceSearcher(serverModule->commonModule()),
    QnAbstractNetworkResourceSearcher(serverModule->commonModule()),
    nx::vms::server::ServerModuleAware(serverModule)
{
}

QnResourcePtr QnPlDlinkResourceSearcher::createResource(
    const QnUuid &resourceTypeId, const QnResourceParams& /*params*/)
{
    QnNetworkResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
    {
        qDebug() << "No resource type for ID = " << resourceTypeId;

        return result;
    }

    if (resourceType->getManufacturer() != manufacturer())
    {
        //qDebug() << "Manufacturer " << resourceType->getManufacturer() << " != " << manufacturer();
        return result;
    }

    result = QnVirtualCameraResourcePtr(new QnPlDlinkResource(serverModule()));
    result->setTypeId(resourceTypeId);

    qDebug() << "Create DLink camera resource. typeID:" << resourceTypeId.toString();

    //result->deserialize(parameters);

    return result;
}

QnResourceList QnPlDlinkResourceSearcher::findResources()
{
    QnResourceList result;

    std::unique_ptr<nx::network::AbstractDatagramSocket> recvSocket(
        nx::network::SocketFactory::createDatagramSocket());
#ifdef Q_OS_WIN
    if (!recvSocket->bind(nx::network::SocketAddress(nx::network::HostAddress::anyHost, 0)))
#else
    if (!recvSocket->bind(nx::network::BROADCAST_ADDRESS, 0))
#endif
        return QnResourceList();

    for (const auto& address:
        nx::network::allLocalAddresses(nx::network::AddressFilter::onlyFirstIpV4))
    {

        if (shouldStop())
            return QnResourceList();

        std::unique_ptr<nx::network::AbstractDatagramSocket> sock(
            nx::network::SocketFactory::createDatagramSocket());
        sock->setReuseAddrFlag(true);

        if (!sock->bind(address.toString(), recvSocket->getLocalAddress().port))
            continue;

        // Sending broadcast.
        constexpr quint16 kPort = 62976;
        for (int r = 0; r < CL_BROAD_CAST_RETRY; ++r)
        {
            sock->sendTo(barequest.data(), barequest.size(), nx::network::BROADCAST_ADDRESS, kPort);

            if (r!=CL_BROAD_CAST_RETRY-1)
                QnSleep::msleep(5);
        }

        // collecting response
        QnSleep::msleep(150);
        while (recvSocket->hasData())
        {
            QByteArray datagram;
            datagram.resize(nx::network::AbstractDatagramSocket::MAX_DATAGRAM_SIZE);

            nx::network::SocketAddress remoteEndpoint;
            const int bytesRead = recvSocket->recvFrom(
                datagram.data(), datagram.size(), &remoteEndpoint);

            if (remoteEndpoint.port != kPort || bytesRead < 32) //< minimum response size
                continue;

            QString name  = QLatin1String("DCS-");

            int iqPos = datagram.indexOf("DCS-");

            if (iqPos < 0)
                continue;

            iqPos += name.length();

            while (iqPos < bytesRead && datagram[iqPos] != (char)0)
            {
                name += QLatin1Char(datagram[iqPos]);
                ++iqPos;
            }

            const unsigned char* data = (unsigned char*)(datagram.data());
            constexpr int kMacAddressOffset = 6;
            const auto mac = nx::utils::MacAddress::fromRawData(data + kMacAddressOffset);

            bool haveToContinue = false;
            for (const QnResourcePtr& res: result)
            {
                const auto networkResource = res.dynamicCast<QnNetworkResource>();
                if (!NX_ASSERT(networkResource))
                    continue;

                if (networkResource->getMAC() == mac)
                {
                    haveToContinue = true;
                    break; //< already found
                }
            }

            if (haveToContinue)
                break;

            const QnPlDlinkResourcePtr resource(new QnPlDlinkResource(serverModule()));

            const QnUuid typeId = qnResTypePool->getLikeResourceTypeId(manufacturer(), name);
            if (typeId.isNull())
                continue;

            const QnResourceData resourceData = dataPool()->data(manufacturer(), name);
            if (resourceData.value<bool>(ResourceDataKey::kForceONVIF))
                continue; //< model forced by ONVIF

            resource->setTypeId(typeId);
            resource->setName(name);
            resource->setModel(name);
            resource->setMAC(mac);
            resource->setHostAddress(remoteEndpoint.address.toString());

            result.push_back(resource);
        }

    }

    return result;
}

QString QnPlDlinkResourceSearcher::manufacturer() const
{
    return QnPlDlinkResource::MANUFACTURE;
}

QList<QnResourcePtr> QnPlDlinkResourceSearcher::checkHostAddr(
    const nx::utils::Url& url, const QAuthenticator& auth, bool isSearchAction)
{
    if (!url.scheme().isEmpty() && isSearchAction)
        return QList<QnResourcePtr>();  // Searching if only host is present, not specific protocol.

    QString host = url.host();
    // #dliman This logic should be removed, url must contain host field here.
    NX_ASSERT(!host.isEmpty());
    if (host.isEmpty())
        host = url.toString(); //< If url is just a host address without a protocol and a port.

    int port = url.port();
    if (port <= 0)
        port = nx::network::http::DEFAULT_HTTP_PORT;

    const auto requestUrl = nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setHost(host)
        .setPort(port)
        .setUserName(auth.user())
        .setPassword(auth.password())
        .setPath("/common/info.cgi")
        .toUrl();

    nx::network::http::StatusCode::Value status = nx::network::http::StatusCode::undefined;
    nx::network::http::BufferType responseBuffer;
    const auto returnCode =
        nx::network::http::downloadFileSync(requestUrl, (int*) &status, &responseBuffer);
    if (returnCode != SystemError::noError || !nx::network::http::StatusCode::isSuccessCode(status))
    {
        NX_DEBUG(this, "Request error, system error: %1, status code: %2",
            returnCode, nx::network::http::StatusCode::toString(status));
        return QList<QnResourcePtr>();
    }

    const QString response = responseBuffer;
    if (response.isEmpty())
        return QList<QnResourcePtr>();

    QStringList lines = response.split(QLatin1String("\r\n"), QString::SkipEmptyParts);

    QString name;
    QString mac;

    for(const QString& line: lines)
    {
        if (line.contains(QLatin1String("name=")))
        {
            name = getValueFromString(line);
        }
        else if (line.contains(QLatin1String("macaddr=")))
        {
            mac = getValueFromString(line);
            mac.replace(QLatin1Char(':'),QLatin1Char('-'));
        }

    }

    if (mac.isEmpty() || name.isEmpty())
        return QList<QnResourcePtr>();

    QnUuid rt = qnResTypePool->getLikeResourceTypeId(manufacturer(), name);
    if (rt.isNull())
        return QList<QnResourcePtr>();

    QnResourceData resourceData = dataPool()->data(manufacturer(), name);
    if (resourceData.value<bool>(ResourceDataKey::kForceONVIF))
        return QList<QnResourcePtr>(); // model forced by ONVIF

    QnNetworkResourcePtr resource ( new QnPlDlinkResource(serverModule()) );

    resource->setTypeId(rt);
    resource->setName(name);
    resource->setMAC(nx::utils::MacAddress(mac));
    (resource.dynamicCast<QnPlDlinkResource>())->setModel(name);
    resource->setHostAddress(host);
    resource->setDefaultAuth(auth);

    //resource->setDiscoveryAddr(iface.address);
    QList<QnResourcePtr> result;
    result << resource;
    return result;
}

#endif // ENABLE_DLINK
