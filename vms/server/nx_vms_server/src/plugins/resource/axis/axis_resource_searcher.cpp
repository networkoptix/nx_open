#ifdef ENABLE_AXIS

#include "axis_resource.h"
#include "axis_resource_searcher.h"

#include <nx/utils/log/log.h>
#include <nx/network/url/url_builder.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <plugins/resource/mdns/mdns_packet.h>
#include <utils/common/credentials.h>

extern QString getValueFromString(const QString& line);

namespace
{
    const int kDefaultAxisTimeout = 4000;
    const QString kTestCredentialsUrl = lit("axis-cgi/param.cgi?action=list&group=root.Network.Bonjour.FriendlyName");
    static const QString kChannelNumberSuffix(lit("_channel_")); //< For physicalId.
    static const QString kUrlChannelNumber(lit("channel"));
}

QnPlAxisResourceSearcher::QnPlAxisResourceSearcher(QnMediaServerModule* serverModule):
    QnAbstractResourceSearcher(serverModule->commonModule()),
    QnAbstractNetworkResourceSearcher(serverModule->commonModule()),
    QnMdnsResourceSearcher(serverModule),
    m_serverModule(serverModule)
{
}

QnResourcePtr QnPlAxisResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& /*params*/)
{
    QnNetworkResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
    {
        NX_DEBUG(this, lit("No resource type for ID %1").arg(resourceTypeId.toString()));

        return result;
    }

    if (resourceType->getManufacture() != manufacture())
    {
        return result;
    }

    result = QnVirtualCameraResourcePtr(new QnPlAxisResource(m_serverModule));
    result->setTypeId(resourceTypeId);

    NX_DEBUG(this, lit("Create Axis camera resource. TypeID %1.").arg(resourceTypeId.toString()));

    return result;

}

QString QnPlAxisResourceSearcher::manufacture() const
{
    return QnPlAxisResource::MANUFACTURE;
}

QList<QnResourcePtr> QnPlAxisResourceSearcher::checkHostAddr(const nx::utils::Url& url, const QAuthenticator& auth, bool isSearchAction)
{
    if( !url.scheme().isEmpty() && isSearchAction )

        return QList<QnResourcePtr>();  //searching if only host is present, not specific protocol

    QString host = url.host();
    int port = url.port();
    if (host.isEmpty())
        host = url.toString(); // in case if url just host address without protocol and port

    if (port < 0)
        port = nx::network::http::DEFAULT_HTTP_PORT;

    nx::utils::Url baseRequestUrl = nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setHost(host)
        .setPort(port)
        .setUserName(auth.user())
        .setPassword(auth.password())
        .setPath("/axis-cgi/param.cgi")
        .toUrl();
    nx::network::http::StatusCode::Value status;

    QByteArray response1;
    nx::network::http::downloadFileSync(nx::network::url::Builder(baseRequestUrl)
        .setQuery("action=list&group=root.Network.Bonjour.FriendlyName").toUrl(),
        (int*) &status, &response1);
    if (response1.isEmpty() || !nx::network::http::StatusCode::isSuccessCode(status))
        return QList<QnResourcePtr>();

    QByteArray response2;
    nx::network::http::downloadFileSync(nx::network::url::Builder(baseRequestUrl)
        .setQuery("action=list&group=root.Network.eth0.MACAddress").toUrl(),
        (int*) &status, &response2);
    if (response2.isEmpty() || !nx::network::http::StatusCode::isSuccessCode(status))
        return QList<QnResourcePtr>();

    QString response = QString(QLatin1String(response1.append(response2)));
    QStringList lines = response.split(QLatin1String("\n"), QString::SkipEmptyParts);

    QString name;
    QString mac;

    for (const QString& line: lines)
    {
        if (line.contains(QLatin1String("root.Network.Bonjour.FriendlyName")))
        {
            name = getValueFromString(line);
            name.replace(QLatin1Char(' '), QString());
        }
        else if (line.contains(QLatin1String("root.Network.eth0.MACAddress")))
        {
            mac = getValueFromString(line);
            mac.replace(QLatin1Char(':'),QLatin1Char('-'));
        }

    }

    name = name.left(name.lastIndexOf(QLatin1Char('-')));
    name.replace(QLatin1Char('-'), QString());

    if (mac.isEmpty() || name.isEmpty())
        return QList<QnResourcePtr>();

    QnUuid typeId = qnResTypePool->getLikeResourceTypeId(manufacture(), name);
    if (typeId.isNull())
        return QList<QnResourcePtr>();

    QnResourceData resourceData = dataPool()->data(manufacture(), name);
    if (resourceData.value<bool>(ResourceDataKey::kForceONVIF))
        return QList<QnResourcePtr>(); // model forced by ONVIF

    QnPlAxisResourcePtr resource(new QnPlAxisResource(m_serverModule));
    resource->setTypeId(typeId);
    resource->setName(name);
    resource->setModel(name);
    resource->setMAC(nx::utils::MacAddress(mac));
    nx::utils::Url finalUrl(url);
    finalUrl.setScheme(QLatin1String("http"));
    finalUrl.setPort(port);
    resource->setUrl(finalUrl.toString());
    resource->setDefaultAuth(auth);

    //resource->setDiscoveryAddr(iface.address);
    QList<QnResourcePtr> result;
    result << resource;

    QUrlQuery urlQuery(url.query());
    if (isSearchAction)
        addMultichannelResources(result);
    else if (urlQuery.hasQueryItem(kUrlChannelNumber))
        setChannelToResource(resource, urlQuery.queryItemValue(kUrlChannelNumber).toInt());

    return result;
}

/**
 * In local networks Axis cameras have two ipv4 addresses for the same network interface:
 * private network address 192.168.x.y (https://tools.ietf.org/html/rfc1918)
 * and link-local address 169.254.w.z. (https://tools.ietf.org/html/rfc3927)
 * The same camera is periodically discovered with each of these addresses
 * (several minutes with one, then several minutes with the other).
 *
 * If incoming address is non link-local ipv4 we return it.
 * If incoming address is link-local ipv4 address we
 *
 * We need to ignore temporary link-local addresses. To do this we use some heuristic.
 *
 * Here we treat a special situation: if camera is discovered with link-local address
 * we wait for kConfidenceInterval time. If discovered ip is still link-local address we set
 * this ip to camera otherwise we set the previous (private network) address stored for the mac.
 */
nx::network::SocketAddress QnPlAxisResourceSearcher::obtainFixedHostAddress(
    nx::utils::MacAddress discoveredMac, nx::network::SocketAddress discoveredAddress)
{
    static const auto kConfidenceInterval = std::chrono::minutes(20);

    const bool isIpv4LinkLocalAddress = discoveredAddress.address.isIpv4LinkLocalNetwork();

    auto& lastNonIpv4LinkLocalTimeMarkedAddress = m_foundNonIpv4LinkLocalAddresses[discoveredMac];
    if (!isIpv4LinkLocalAddress)
        lastNonIpv4LinkLocalTimeMarkedAddress = TimeMarkedAddress(discoveredAddress);

    // We use discoveredAddress if
    // 1. discovered address is non link-local OR
    // 2. non link-local address have never been discovered OR
    // 3. non link-local address was discovered long ago last time.
    // Otherwise we use lastNonLinkLocalTimeMarkedAddress.
    const bool useDiscoveredAddress = !isIpv4LinkLocalAddress //< 1
        || !lastNonIpv4LinkLocalTimeMarkedAddress.elapsedTimer.isValid() //< 2
        || lastNonIpv4LinkLocalTimeMarkedAddress.elapsedTimer.hasExpired(kConfidenceInterval); //< 3

    return useDiscoveredAddress ? discoveredAddress : lastNonIpv4LinkLocalTimeMarkedAddress.address;
}

QList<QnNetworkResourcePtr> QnPlAxisResourceSearcher::processPacket(
    QnResourceList& result,
    const QByteArray& responseData,
    const QHostAddress& /*discoveryAddress*/,
    const QHostAddress& foundHostAddress )
{
    QString smac;
    QString name;

    QList<QnNetworkResourcePtr> local_results;

    int iqpos = responseData.indexOf("AXIS");

    if (iqpos<0)
        return local_results;

    QByteArray prefix2("axis-00");
    int macpos = responseData.indexOf("- 00", iqpos);
    if (macpos < 0) {
        macpos = responseData.indexOf("- AC", iqpos);
        if (macpos < 0)
            return local_results;
        prefix2 = QByteArray("axis-ac");
    }
    macpos += 2;

    for (int i = iqpos; i < macpos; i++)
    {
        const unsigned char c = responseData.at(i);
        if (c < 0x20 || c == 0x7F) // Control char.
            name += QLatin1Char('?');
        else
            name += QLatin1Char(c); //< Assume Latin-1 encoding.
    }

    int macpos2 = responseData.indexOf(prefix2, macpos);
    if (macpos2 > 0)
        macpos = macpos2 + 5; // replace real MAC to virtual MAC if exists

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

    for(const QnResourcePtr& res: result)
    {
        QnNetworkResourcePtr net_res = res.dynamicCast<QnNetworkResource>();

        if (net_res->getMAC().toString() == smac) {
            return local_results; // already found;
        }
    }

    QnResourceData resourceData = dataPool()->data(manufacture(), name);
    if (resourceData.value<bool>(ResourceDataKey::kForceONVIF))
        return local_results; // model forced by ONVIF

    QnPlAxisResourcePtr resource (new QnPlAxisResource(m_serverModule));

    QnUuid rt = qnResTypePool->getLikeResourceTypeId(manufacture(), name);
    if (rt.isNull())
        return local_results;

    resource->setTypeId(rt);
    resource->setName(name);
    resource->setModel(name);
    auto mac = nx::utils::MacAddress(smac);
    resource->setMAC(mac);

    quint16 port = nx::network::http::DEFAULT_HTTP_PORT;
    QnMdnsPacket packet;
    if (packet.fromDatagram(responseData))
    {
        auto rrsToInspect = packet.answerRRs + packet.additionalRRs;

        for (const auto& record: rrsToInspect)
        {
            if (record.recordType == QnMdnsPacket::kSrvRecordType)
            {
                QnMdnsSrvData srv;
                srv.decode(record.data);
                port = srv.port;
            }
        }
    }

    QUrl url;
    url.setScheme(lit("http"));

    auto fixedIpPort = obtainFixedHostAddress(
        mac, nx::network::SocketAddress(foundHostAddress.toString(), port));

    url.setHost(fixedIpPort.address.toString());
    url.setPort(fixedIpPort.port);

    resource->setUrl(url.toString());

    auto auth = determineResourceCredentials(resource);
    resource->setDefaultAuth(auth);

    local_results.push_back(resource);

    addMultichannelResources(local_results);

    return local_results;
}

bool QnPlAxisResourceSearcher::testCredentials(
    const QUrl& url,
    const QAuthenticator& auth) const
{
    auto host = url.host();
    auto port = url.port(nx::network::http::DEFAULT_HTTP_PORT);

    CLHttpStatus status;
    auto response = downloadFile(status, kTestCredentialsUrl, host, port, kDefaultAxisTimeout, auth);

    if (status != CL_HTTP_SUCCESS)
        return false;

    return true;
}

QAuthenticator QnPlAxisResourceSearcher::determineResourceCredentials(
    const QnSecurityCamResourcePtr& resource) const
{
    if (!resource)
        return QAuthenticator();

    auto existingResource = resourcePool()->getNetResourceByPhysicalId(resource->getPhysicalId());
    if (existingResource)
        return existingResource->getAuth();

    if (discoveryMode() != DiscoveryMode::fullyEnabled)
        return QAuthenticator();

    auto resData = dataPool()->data(resource->getVendor(), resource->getModel());
    auto possibleCredentials = resData.value<QList<nx::vms::common::Credentials>>(
        ResourceDataKey::kPossibleDefaultCredentials);

    for (const auto& credentials: possibleCredentials)
    {
        const auto& auth = credentials.toAuthenticator();
        if (testCredentials(resource->getUrl(), auth))
            return auth;
    }

    return QAuthenticator();
}

void QnPlAxisResourceSearcher::setChannelToResource(const QnPlAxisResourcePtr& resource, int value)
{
    QUrl url(resource->getUrl());
    QUrlQuery q(url.query());
    q.removeAllQueryItems(kUrlChannelNumber);
    q.addQueryItem(kUrlChannelNumber, QString::number(value));
    url.setQuery(q);
    resource->setUrl(url.toString());

    const auto physicalId = resource->getPhysicalId();
    if (physicalId.indexOf(kChannelNumberSuffix) == -1)
    {
        resource->setPhysicalId(physicalId + kChannelNumberSuffix + QString::number(value));
        resource->setName(resource->getName() + QString(QLatin1String("-channel %1")).arg(value));
        // Channel enumeration in range [1..n].
        resource->setGroupId(physicalId + kChannelNumberSuffix + QString::number(1));
    }
}

template <typename T>
void QnPlAxisResourceSearcher::addMultichannelResources(QList<T>& result)
{
    QnPlAxisResourcePtr firstResource = result.first().template dynamicCast<QnPlAxisResource>();

    uint channels = 1;
    if (firstResource->hasDefaultProperty(QLatin1String("channelsAmount")))
    {
        QString val = firstResource->getProperty(QLatin1String("channelsAmount"));
        channels = val.toUInt();
    }
    if (channels > 1)
    {
        QString physicalId = firstResource->getPhysicalId();
        firstResource->setDefaultGroupName(physicalId);
        firstResource->setGroupId(physicalId);

        setChannelToResource(firstResource, 1);

        for (uint i = 2; i <= channels; ++i)
        {
            QnPlAxisResourcePtr resource (new QnPlAxisResource(m_serverModule));

            QnUuid rt = qnResTypePool->getLikeResourceTypeId(manufacture(), firstResource->getName());
            if (rt.isNull())
                return;

            resource->setTypeId(rt);
            resource->setName(firstResource->getModel());
            resource->setModel(firstResource->getModel());
            resource->setMAC(firstResource->getMAC());
            resource->setDefaultGroupName(physicalId);
            resource->setGroupId(physicalId);

            auto auth = firstResource->getAuth();
            if (!auth.isNull())
                resource->setDefaultAuth(auth);

            resource->setUrl(firstResource->getUrl());

            setChannelToResource(resource, i);

            result.push_back(resource);
        }
    }
}

#endif // #ifdef ENABLE_AXIS
