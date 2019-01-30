#ifdef ENABLE_ISD

#include <nx/utils/url.h>
#include <core/resource_management/resource_pool.h>
#include "core/resource/camera_resource.h"
#include "isd_resource_searcher.h"
#include "isd_resource.h"
#include <nx/network/url/url_builder.h>
#include <nx/network/http/http_types.h>
#include "core/resource/resource_data.h"
#include "core/resource_management/resource_data_pool.h"
#include "common/common_module.h"
#include <utils/common/credentials.h>
#include <plugins/resource/mdns/mdns_packet.h>
#include <nx/utils/log/log.h>

using nx::vms::common::Credentials;

extern QString getValueFromString(const QString& line);

using DefaultCredentialsList = QList<Credentials>;

namespace {
const QString kParamUrlPath("/api/param.cgi");
const QString kIsdMacAddressParamName("Network.1.MacAddress");
const QString kIsdBrandParamName("General.Brand.CompanyName");
const QString kIsdModelParamName("General.Brand.ModelName");
const QString kIsdFullVendorName("Innovative Security Designs");
const QString kDwFullVendorName("Digital Watchdog");
const QString kIsdDefaultResType("ISDcam");
const QString kUpnpBasicDeviceType("Basic");

const QLatin1String kDefaultIsdUsername( "root" );
const QLatin1String kDefaultIsdPassword( "admin" );

static QByteArray extractWord(int index, const QByteArray& rawData)
{
    int endIndex = index;
    for (;endIndex < rawData.size() && rawData.at(endIndex) != ' '; ++endIndex);
    return rawData.mid(index, endIndex - index);
}

} // namespace

QnPlISDResourceSearcher::QnPlISDResourceSearcher(QnMediaServerModule* serverModule):
    QnAbstractResourceSearcher(serverModule->commonModule()),
    QnAbstractNetworkResourceSearcher(serverModule->commonModule()),
    SearchAutoHandler(serverModule->upnpDeviceSearcher(), kUpnpBasicDeviceType),
    nx::vms::server::ServerModuleAware(serverModule)
{
    NX_DEBUG(this, "Constructed");
    serverModule->mdnsListener()->registerConsumer((std::uintptr_t) this);
}

QnPlISDResourceSearcher::~QnPlISDResourceSearcher()
{
    NX_DEBUG(this, "Destructed");
}

QnResourcePtr QnPlISDResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& /*params*/)
{
    QnNetworkResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);
    if (resourceType.isNull())
    {
        NX_DEBUG(this, lm("No resource type for %1").arg(resourceTypeId));
        return result;
    }

    if (resourceType->getManufacture() != manufacture())
        return result;

    result = QnVirtualCameraResourcePtr(new QnPlIsdResource(serverModule()));
    result->setTypeId(resourceTypeId);

    NX_DEBUG(this, lm("Create resource with type %1").arg(resourceTypeId));
    return result;
}

QString QnPlISDResourceSearcher::manufacture() const
{
    return QnPlIsdResource::MANUFACTURE;
}

QList<QnResourcePtr> QnPlISDResourceSearcher::checkHostAddr(
    const nx::utils::Url& url,
    const QAuthenticator& authOriginal,
    bool isSearchAction)
{
    if( !url.scheme().isEmpty() && isSearchAction )
        return QList<QnResourcePtr>();  //searching if only host is present, not specific protocol

    bool needRecheck =
        authOriginal.user().isEmpty() &&
        authOriginal.password().isEmpty();

    if (!needRecheck)
    {
        return checkHostAddrInternal(url, authOriginal);
    }
    else
    {
        QList<QnResourcePtr> resList;
        auto resData = dataPool()->data(manufacture(), lit("*"));
        auto possibleCreds = resData.value<DefaultCredentialsList>(
            ResourceDataKey::kPossibleDefaultCredentials);

        possibleCreds << Credentials(kDefaultIsdUsername, kDefaultIsdPassword);

        for (const auto& creds: possibleCreds)
        {
            if ( creds.user.isEmpty() || creds.password.isEmpty())
                continue;

            QAuthenticator auth = creds.toAuthenticator();
            resList = checkHostAddrInternal(url, auth);
            if (!resList.isEmpty())
                break;
        }

        return resList;
    }

}

QnResourceList QnPlISDResourceSearcher::findResources()
{

    QnResourceList upnpResults;
    {
        QnMutexLocker lock(&m_mutex);
        upnpResults = m_foundUpnpResources;
        m_foundUpnpResources.clear();
        m_alreadyFoundMacAddresses.clear();
    }

    QnResourceList mdnsResults;
    auto consumerData = serverModule()->mdnsListener()->getData((std::uintptr_t) this);
    consumerData->forEachEntry(
        [this, &mdnsResults, &upnpResults](
            const QString& remoteAddress,
            const QString& /*localAddress*/,
            const QByteArray& responseData)
        {
            auto resource = processMdnsResponse(responseData, remoteAddress, upnpResults);
            if (resource)
                mdnsResults << resource;
        });

    const auto totalResults = upnpResults + mdnsResults;
    NX_DEBUG(this, lm("Found resources: %1 UPnP + %2 MDNS = %3 total").args(
        upnpResults.size(), mdnsResults.size(), totalResults.size()));
    return totalResults;
}

QList<QnResourcePtr> QnPlISDResourceSearcher::checkHostAddrInternal(
    const nx::utils::Url &url,
    const QAuthenticator &authOriginal)
{

    if (shouldStop())
        return QList<QnResourcePtr>();

    QAuthenticator auth( authOriginal );

    QString host = url.host();
    int port = url.port( nx::network::http::DEFAULT_HTTP_PORT );
    if (host.isEmpty())
        host = url.toString();

    QString name;
    QString vendor;

    QUrlQuery query;
    query.addQueryItem("req", kIsdBrandParamName);
    query.addQueryItem("req", kIsdModelParamName);
    auto requestUrl = nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setHost(host)
        .setPort(port)
        .setUserName(auth.user())
        .setPassword(auth.password())
        .setPath(kParamUrlPath)
        .setQuery(std::move(query))
        .toUrl();

    NX_VERBOSE(this, "Sending request: %1", requestUrl);
    nx::network::http::StatusCode::Value status = nx::network::http::StatusCode::undefined;
    nx::network::http::BufferType responseBuffer;
    auto returnCode =
        nx::network::http::downloadFileSync(requestUrl, (int*) &status, &responseBuffer);
    if (returnCode != SystemError::noError || !nx::network::http::StatusCode::isSuccessCode(status))
    {
        NX_DEBUG(this, "Request %1 error, system error: %2, status code: %3", requestUrl,
            SystemError::toString(returnCode), nx::network::http::StatusCode::toString(status));
        return QList<QnResourcePtr>();
    }

    for (const QByteArray& line: responseBuffer.split(L'\n'))
    {
        if (line.startsWith(kIsdModelParamName.toLatin1())) {
            name = getValueFromString(QString::fromUtf8(line)).trimmed();
            cleanupSpaces(name);
        }
        else if (line.startsWith(kIsdBrandParamName.toLatin1())) {
            vendor = getValueFromString(QString::fromUtf8(line)).trimmed();
        }
    }

    // 'Digital Watchdog' (with space) is actually ISD cameras. Without space it's old DW cameras
    if (name.isEmpty() || (vendor != kIsdFullVendorName && vendor != kDwFullVendorName))
        return QList<QnResourcePtr>();

    if (shouldStop())
        return QList<QnResourcePtr>();

    query.clear();
    query.addQueryItem("req", kIsdMacAddressParamName);
    requestUrl = nx::network::url::Builder(requestUrl)
        .setPath(kParamUrlPath)
        .setQuery(std::move(query))
        .toUrl();

    NX_VERBOSE(this, "Sending request: %1", requestUrl);
    responseBuffer.clear();
    returnCode = nx::network::http::downloadFileSync(requestUrl, (int*) &status, &responseBuffer);
    if (returnCode != SystemError::noError || !nx::network::http::StatusCode::isSuccessCode(status))
    {
        NX_DEBUG(this, "Request %1 error, system error: %2, status code: %3",
            requestUrl, returnCode, nx::network::http::StatusCode::toString(status));
        return QList<QnResourcePtr>();
    }

    QString mac = responseBuffer;
    cleanupSpaces(mac);
    if (mac.isEmpty() || name.isEmpty())
        return QList<QnResourcePtr>();

    mac = getValueFromString(mac).trimmed();

    if (mac.length() > 17 && mac.endsWith(QLatin1Char('0')))
        mac.chop(mac.length() - 17);

    QnUuid rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (rt.isNull()) {
        rt = qnResTypePool->getResourceTypeId(manufacture(), kIsdDefaultResType);
        if (rt.isNull())
        {
            NX_ASSERT(false, lm("No resource type for %1").arg(name));
            return QList<QnResourcePtr>();
        }
    }

    QnResourceData resourceData = dataPool()->data(manufacture(), name);
    if (resourceData.value<bool>(ResourceDataKey::kForceONVIF))
    {
        NX_VERBOSE(this, lm("ONVIF is forced for vendor: %1, model: %2").args(manufacture(), name));
        return QList<QnResourcePtr>();
    }

    QnPlIsdResourcePtr resource (new QnPlIsdResource(serverModule()));
    auto isDW = resourceData.value<bool>(ResourceDataKey::kIsdDwCam);

    vendor = isDW ? kDwFullVendorName : vendor;
    name = vendor == kIsdFullVendorName ?
        lit("ISD-") + name :
        name;

    resource->setTypeId(rt);
    resource->setVendor(vendor);
    resource->setName(name);
    resource->setModel(name);
    resource->setMAC(nx::utils::MacAddress(mac));
    resource->setDefaultAuth(auth);
    if (port == 80)
        resource->setHostAddress(host);
    else
        resource->setUrl(QString(lit("http://%1:%2"))
            .arg(host)
            .arg(port));

    NX_VERBOSE(this, lm("Checked resource vendor: %1, model: %2, mac: %3, url: %4")
        .args(vendor, name, mac, resource->getUrl()));

    //resource->setDiscoveryAddr(iface.address);
    QList<QnResourcePtr> result;
    result << resource;
    return result;
}

void QnPlISDResourceSearcher::cleanupSpaces(QString& rowWithSpaces) const
{
    rowWithSpaces.replace(QLatin1Char(' '), QString());
    rowWithSpaces.replace(QLatin1Char('\r'), QString());
    rowWithSpaces.replace(QLatin1Char('\n'), QString());
    rowWithSpaces.replace(QLatin1Char('\t'), QString());
}

bool QnPlISDResourceSearcher::isDwOrIsd(const QString &vendorName, const QString& model) const
{
    if (vendorName.toUpper().startsWith(manufacture()))
    {
        return true;
    }
    else if(vendorName.toLower().trimmed() == lit("digital watchdog") ||
        vendorName.toLower().trimmed() == lit("digitalwatchdog"))
    {
        QnResourceData resourceData = dataPool()->data("DW", model);
        if (resourceData.value<bool>(ResourceDataKey::kIsdDwCam))
            return true;
    }

    NX_VERBOSE(this, lm("Not a DW or ISD vendor: %1, model: %2").args(vendorName, model));
    return false;
}

QnResourcePtr QnPlISDResourceSearcher::processMdnsResponse(
    const QByteArray& responseData,
    const QString& mdnsRemoteAddress,
    const QnResourceList& alreadyFoundResources)
{
    QString name(kIsdDefaultResType);
    if (!responseData.contains("ISD"))
    {
        // check for new ISD models. it has been rebranded
        int modelPos = responseData.indexOf("DWCA-");
        if (modelPos == -1)
            modelPos = responseData.indexOf("DWCS-");
        if (modelPos == -1)
            modelPos = responseData.indexOf("DWEA-");
        if (modelPos == -1)
            return QnResourcePtr(); // not found

        if (modelPos != -1)
            name = extractWord(modelPos, responseData);

        NX_VERBOSE(this, lm("MDNS from %1 with name: %2").args(
            mdnsRemoteAddress, name));
    }

    int macpos = responseData.indexOf("macaddress=");
    if (macpos < 0)
        return QnResourcePtr();

    macpos += QString(QLatin1String("macaddress=")).length();
    if (macpos + 12 > responseData.size())
        return QnResourcePtr();

    QByteArray smac;
    for (int i = 0; i < 12; i++)
    {
        if (i > 0 && i % 2 == 0)
            smac += '-';

        smac += responseData[macpos + i];
    }

    NX_VERBOSE(this, lm("MDNS from %1 with name: %2, MAC: %3").args(
        mdnsRemoteAddress, name, smac));

    quint16 port = nx::network::http::DEFAULT_HTTP_PORT;
    QnMdnsPacket packet;
    if (packet.fromDatagram(responseData))
    {
        for (const auto& answer: packet.answerRRs)
        {
            if(answer.recordType == QnMdnsPacket::kSrvRecordType)
            {
                QnMdnsSrvData srvData;
                srvData.decode(answer.data);
                port = srvData.port;
                break;
            }
        }
    }
    else
    {
        NX_DEBUG(this, lm("MDNS from %1: Unable to parse packet").arg(mdnsRemoteAddress));
    }

    smac = smac.toUpper();

    for (const QnResourcePtr& res: alreadyFoundResources)
    {
        QnNetworkResourcePtr netRes = res.dynamicCast<QnNetworkResource>();

        if (netRes->getMAC().toString() == smac)
            return QnResourcePtr(); // already found;
    }

    QnPlIsdResourcePtr resource (new QnPlIsdResource(serverModule()));

    QAuthenticator cameraAuth;
    if (auto existingRes = serverModule()->resourcePool()->getResourceByMacAddress( smac ) )
        cameraAuth = existingRes->getAuth();

    QnUuid rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (rt.isNull())
    {
        rt = qnResTypePool->getResourceTypeId(manufacture(), kIsdDefaultResType);
        if (rt.isNull())
        {
            NX_ASSERT(false, lm("No resource type for %1").arg(name));
            return QnResourcePtr();
        }
    }

    QnResourceData resourceData = dataPool()->data(manufacture(), name);
    if (resourceData.value<bool>(ResourceDataKey::kForceONVIF))
    {
        NX_VERBOSE(this, lm("ONVIF is forced for vendor: %1, model: %2").args(manufacture(), name));
        return QnResourcePtr();
    }

    resource->setTypeId(rt);
    resource->setName(name);
    resource->setModel(name);
    resource->setMAC(nx::utils::MacAddress(smac));

    nx::utils::Url url;
    url.setScheme(lit("http"));
    url.setHost(mdnsRemoteAddress);
    url.setPort(port);

    resource->setUrl(url.toString());

    if (name == lit("DWcam"))
        resource->setDefaultAuth(lit("admin"), lit("admin"));
    else
        resource->setDefaultAuth(lit("root"), lit("admin"));

    if(!cameraAuth.isNull())
    {
        resource->setAuth(cameraAuth);
    }
    else
    {
        auto resData = dataPool()->data(manufacture(), name);
        auto possibleCreds = resData.value<DefaultCredentialsList>(
            ResourceDataKey::kPossibleDefaultCredentials);

        for (const auto& creds: possibleCreds)
        {
            QAuthenticator auth = creds.toAuthenticator();
            nx::utils::Url url(lit("//") + mdnsRemoteAddress);
            url.setPort(port);
            if (testCredentials(url, auth))
            {
                resource->setDefaultAuth(auth);
                break;
            }
        }
    }

    return resource;
}

bool QnPlISDResourceSearcher::processPacket(
    const QHostAddress& /*discoveryAddr*/,
    const nx::network::SocketAddress& deviceEndpoint,
    const nx::network::upnp::DeviceInfo& devInfo,
    const QByteArray& /*xmlDevInfo*/)
{
    if (!isDwOrIsd(devInfo.manufacturer, devInfo.modelName))
        return false;

    NX_VERBOSE(this, lm("UPnP from %1 vendor: %2, model: %3").args(
        deviceEndpoint, devInfo.manufacturer, devInfo.modelName));

    nx::utils::MacAddress cameraMAC(devInfo.serialNumber);
    QString model(devInfo.modelName);
    QnNetworkResourcePtr existingRes = serverModule()->resourcePool()->getResourceByMacAddress(devInfo.serialNumber);
    QAuthenticator cameraAuth;

    if ( existingRes )
        cameraMAC = existingRes->getMAC();

    if (existingRes)
    {
        auto existAuth = existingRes->getAuth();
        cameraAuth = existAuth;
    }
    else
    {
        auto resData = dataPool()->data(manufacture(), model);
        auto possibleCreds = resData.value<DefaultCredentialsList>(
            ResourceDataKey::kPossibleDefaultCredentials);

        cameraAuth.setUser(kDefaultIsdUsername);
        cameraAuth.setPassword(kDefaultIsdPassword);

        for (const auto& creds: possibleCreds)
        {
            QAuthenticator auth = creds.toAuthenticator();
            nx::utils::Url url(lit("//") + deviceEndpoint.address.toString());
            if (testCredentials(url, auth))
            {
                cameraAuth = auth;
                break;
            }
        }
    }

    {
        QnMutexLocker lock(&m_mutex);

        if (m_alreadyFoundMacAddresses.find(cameraMAC.toString()) == m_alreadyFoundMacAddresses.end())
        {
            m_alreadyFoundMacAddresses.insert(cameraMAC.toString());
            createResource( devInfo, cameraMAC, cameraAuth, m_foundUpnpResources);
        }
        else
        {
            NX_VERBOSE(this, lm("UPnP from %1 vendor: %2, model: %3, MAC: %4 is known")
                .args(deviceEndpoint, devInfo.manufacturer, devInfo.modelName, cameraMAC));
        }
    }

    return true;
}

void QnPlISDResourceSearcher::createResource(
    const nx::network::upnp::DeviceInfo& devInfo,
    const nx::utils::MacAddress& mac,
    const QAuthenticator& auth,
    QnResourceList& result )
{

    QnUuid rt = qnResTypePool->getResourceTypeId(manufacture(), devInfo.modelName);
    if (rt.isNull())
    {
        rt = qnResTypePool->getResourceTypeId(manufacture(), kIsdDefaultResType);
        if (rt.isNull())
        {
            NX_ASSERT(false, lm("No resource type for %1").arg(devInfo.modelName));
            return;
        }
    }

    QnResourceData resourceData = dataPool()->data(devInfo.manufacturer, devInfo.modelName);
    if (resourceData.value<bool>(ResourceDataKey::kForceONVIF))
    {
        NX_VERBOSE(this, lm("ONVIF is forced for vendor: %1, model: %2").args(
            devInfo.manufacturer, devInfo.modelName));
        return;
    }

    auto isDW = resourceData.value<bool>(ResourceDataKey::kIsdDwCam);
    auto vendor = isDW ? kDwFullVendorName :
        (devInfo.manufacturer == lit("ISD") || devInfo.manufacturer == kIsdFullVendorName) ?
            manufacture() :
            devInfo.manufacturer;

    auto name = (vendor == manufacture()) ?
        lit("ISD-") + devInfo.modelName :
        devInfo.modelName;

    QnPlIsdResourcePtr resource(new QnPlIsdResource(serverModule()));

    resource->setTypeId(rt);
    resource->setVendor(vendor);
    resource->setName(name);
    resource->setModel(devInfo.modelName);
    resource->setUrl(devInfo.presentationUrl);
    resource->setMAC(mac);

    if (!auth.isNull()) {
        resource->setDefaultAuth(auth);
    } else {
        QAuthenticator defaultAuth;
        defaultAuth.setUser(kDefaultIsdUsername);
        defaultAuth.setPassword(kDefaultIsdPassword);
        resource->setDefaultAuth(defaultAuth);
    }

    result << resource;
    NX_VERBOSE(this, lm("Created resource vendor: %1, model: %2, mac: %3, url: %4")
        .args(vendor, devInfo.modelName, mac, resource->getUrl()));
}

bool QnPlISDResourceSearcher::testCredentials(const nx::utils::Url &url, const QAuthenticator &auth)
{
    QUrlQuery query;
    query.addQueryItem("req", kIsdBrandParamName);
    query.addQueryItem("req", kIsdModelParamName);
    auto requestUrl = nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setHost(url.host())
        .setPort(url.port(nx::network::http::DEFAULT_HTTP_PORT))
        .setUserName(auth.user())
        .setPassword(auth.password())
        .setPath(kParamUrlPath)
        .setQuery(std::move(query))
        .toUrl();

    if (requestUrl.host().isEmpty())
    {
        NX_DEBUG(this, "Got URL %1 with empty host", requestUrl);
        return false;
    }

    NX_VERBOSE(this, "Testing credentials: %1", requestUrl);

    nx::network::http::StatusCode::Value status = nx::network::http::StatusCode::undefined;
    nx::network::http::BufferType responseBuffer;
    auto returnCode =
        nx::network::http::downloadFileSync(requestUrl, (int*) &status, &responseBuffer);

    return returnCode == SystemError::noError
        && nx::network::http::StatusCode::isSuccessCode(status);
}

bool QnPlISDResourceSearcher::isEnabled() const
{
    return discoveryMode() != DiscoveryMode::disabled;
}

#endif // #ifdef ENABLE_ISD
