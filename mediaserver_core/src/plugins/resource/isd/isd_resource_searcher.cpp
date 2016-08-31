#ifdef ENABLE_ISD

#include <core/resource_management/resource_pool.h>
#include "core/resource/camera_resource.h"
#include "isd_resource_searcher.h"
#include "isd_resource.h"
#include <nx/network/http/httptypes.h>
#include "core/resource/resource_data.h"
#include "core/resource_management/resource_data_pool.h"
#include "common/common_module.h"
#include <utils/common/credentials.h>
#include <plugins/resource/mdns/mdns_packet.h>


extern QString getValueFromString(const QString& line);

typedef QList<QnCredentials> DefaultCredentialsList;

namespace
{
    unsigned int kDefaultIsdHttpTimeout = 4000;
    const QString kIsdModelInfoUrl("/api/param.cgi?req=General.Brand.CompanyName&req=General.Brand.ModelName");
    const QString kIsdMacAddressInfoUrl("/api/param.cgi?req=Network.1.MacAddress");
    const QString kIsdBrandParamName("General.Brand.CompanyName");
    const QString kIsdModelParamName("General.Brand.ModelName");
    const QString kIsdFullVendorName("Innovative Security Designs");
    const QString kDwFullVendorName("Digital Watchdog");
    const QString kIsdDefaultResType("ISDcam");
	const QString kUpnpBasicDeviceType("Basic");

    const QLatin1String kDefaultIsdUsername( "root" );
    const QLatin1String kDefaultIsdPassword( "admin" );
}

QnPlISDResourceSearcher::QnPlISDResourceSearcher()
{
    QnMdnsListener::instance()->registerConsumer((std::uintptr_t) this);
	nx_upnp::DeviceSearcher::instance()->registerHandler(this, kUpnpBasicDeviceType);
}

QnResourcePtr QnPlISDResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& /*params*/)
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

    result = QnVirtualCameraResourcePtr( new QnPlIsdResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create ISD camera resource. typeID:" << resourceTypeId.toString(); // << ", Parameters: " << parameters;
    //result->deserialize(parameters);

    return result;

}

QString QnPlISDResourceSearcher::manufacture() const
{
    return QnPlIsdResource::MANUFACTURE;
}

QList<QnResourcePtr> QnPlISDResourceSearcher::checkHostAddr(
    const QUrl& url,
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
        auto resData = qnCommon->dataPool()->data(manufacture(), lit("*"));
        auto possibleCreds = resData.value<DefaultCredentialsList>(
            Qn::POSSIBLE_DEFAULT_CREDENTIALS_PARAM_NAME);

        QnCredentials defaultCreds;
        defaultCreds.user = kDefaultIsdUsername;
        defaultCreds.password = kDefaultIsdPassword;
        possibleCreds << defaultCreds;

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

QnResourceList QnPlISDResourceSearcher::findResources(void)
{
	
	QnResourceList upnpResults;
    QnResourceList mdnsResults;

	{
		QnMutexLocker lock(&m_mutex);
		upnpResults = m_foundUpnpResources;
		m_foundUpnpResources.clear();
		m_alreadFoundMacAddresses.clear();
	}

    auto mdnsDataList = QnMdnsListener::instance()->getData((std::uintptr_t) this);
    for (const auto& response: mdnsDataList)
    {
        auto resource = processMdnsResponse(response, upnpResults);
        if (resource)
            mdnsResults << resource;
    }

    return upnpResults + mdnsResults;
}


QList<QnResourcePtr> QnPlISDResourceSearcher::checkHostAddrInternal(
    const QUrl &url,
    const QAuthenticator &authOriginal)
{

    QAuthenticator auth( authOriginal );

    QString host = url.host();
    int port = url.port( nx_http::DEFAULT_HTTP_PORT );
    if (host.isEmpty())
        host = url.toString();

    QString name;
    QString vendor;

    CLHttpStatus status;
    QByteArray data = downloadFile(
        status,
        kIsdModelInfoUrl,
        host,
        port,
        kDefaultIsdHttpTimeout,
        auth);

    for (const QByteArray& line: data.split(L'\n'))
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


    QString mac = QString(QLatin1String(
        downloadFile(
            status,
            kIsdMacAddressInfoUrl,
            host,
            port,
            kDefaultIsdHttpTimeout,
            auth)));

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
            return QList<QnResourcePtr>();
    }

    QnResourceData resourceData = qnCommon->dataPool()->data(manufacture(), name);

    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return QList<QnResourcePtr>();

    QnPlIsdResourcePtr resource ( new QnPlIsdResource() );
    auto isDW = resourceData.value<bool>(Qn::DW_REBRANDED_TO_ISD_MODEL);

    vendor = isDW ? kDwFullVendorName : vendor;
    name = vendor == kIsdFullVendorName ?
        lit("ISD-") + name :
        name;

    resource->setTypeId(rt);
    resource->setVendor(vendor);
    resource->setName(name);
    resource->setModel(name);
    resource->setMAC(QnMacAddress(mac));
    resource->setDefaultAuth(auth);
    if (port == 80)
        resource->setHostAddress(host);
    else
        resource->setUrl(QString(lit("http://%1:%2"))
            .arg(host)
            .arg(port));

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

QString extractWord(int index, const QByteArray& rawData)
{
    int endIndex = index;
    for (;endIndex < rawData.size() && rawData.at(endIndex) != ' '; ++endIndex);
    return QString::fromLatin1(rawData.mid(index, endIndex - index));
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
        QnResourceData resourceData = qnCommon->dataPool()->data("DW", model);
        if (resourceData.value<bool>(Qn::DW_REBRANDED_TO_ISD_MODEL))
            return true;
    }
    return false;
}



QnResourcePtr QnPlISDResourceSearcher::processMdnsResponse(
    const QnMdnsListener::ConsumerData& mdnsResponse,
    const QnResourceList& alreadyFoundResources)
{

    auto responseData = mdnsResponse.response;

    QString smac;
    QString name(lit("ISDcam"));

    if (!responseData.contains("ISD")) {
        // check for new ISD models. it has been rebranded
        int modelPos = responseData.indexOf("DWCA-");
        if (modelPos == -1)
            modelPos = responseData.indexOf("DWCS-");
        if (modelPos == -1)
            modelPos = responseData.indexOf("DWEA-");
        if (modelPos == -1 && !responseData.contains("ISD"))
            return QnResourcePtr(); // not found

        if(modelPos != -1)
            name = extractWord(modelPos, responseData);
    }

    int macpos = responseData.indexOf("macaddress=");
    if (macpos < 0)
        return QnResourcePtr();

    macpos += QString(QLatin1String("macaddress=")).length();
    if (macpos + 12 > responseData.size())
        return QnResourcePtr();

    for (int i = 0; i < 12; i++)
    {
        if (i > 0 && i % 2 == 0)
            smac += QLatin1Char('-');

        smac += QLatin1Char(responseData[macpos + i]);
    }

    quint16 port = nx_http::DEFAULT_HTTP_PORT;
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
        qDebug() << "There are errors in mdns packet parsing";
    }

    smac = smac.toUpper();

    for (const QnResourcePtr& res: alreadyFoundResources)
    {
        QnNetworkResourcePtr netRes = res.dynamicCast<QnNetworkResource>();

        if (netRes->getMAC().toString() == smac)
            return QnResourcePtr(); // already found;
    }

    QnPlIsdResourcePtr resource ( new QnPlIsdResource() );


    QAuthenticator cameraAuth;
    if (auto existingRes = qnResPool->getResourceByMacAddress( smac ) )
        cameraAuth = existingRes->getAuth();

    QnUuid rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (rt.isNull())
    {
        rt = qnResTypePool->getResourceTypeId(manufacture(), lit("ISDcam"));
        if (rt.isNull())
            return QnResourcePtr();
    }

    QnResourceData resourceData = qnCommon->dataPool()->data(manufacture(), name);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return QnResourcePtr();

    resource->setTypeId(rt);
    resource->setName(name);
    resource->setModel(name);
    resource->setMAC(QnMacAddress(smac));

    QUrl url;
    url.setScheme(lit("http"));
    url.setHost(mdnsResponse.remoteAddress);
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
        auto resData = qnCommon->dataPool()->data(manufacture(), name);
        auto possibleCreds = resData.value<DefaultCredentialsList>(
            Qn::POSSIBLE_DEFAULT_CREDENTIALS_PARAM_NAME);

        for (const auto& creds: possibleCreds)
        {
            QAuthenticator auth = creds.toAuthenticator();
            QUrl url(lit("//") + mdnsResponse.remoteAddress);
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
    const QHostAddress& discoveryAddr,
    const SocketAddress& deviceEndpoint,
    const nx_upnp::DeviceInfo& devInfo,
    const QByteArray& /*xmlDevInfo*/)
{

	QN_UNUSED(discoveryAddr);

    if (!isDwOrIsd(devInfo.manufacturer, devInfo.modelName))
        return false;

    QnMacAddress cameraMAC(devInfo.serialNumber);
    QString model(devInfo.modelName);
    QnNetworkResourcePtr existingRes = qnResPool->getResourceByMacAddress( devInfo.serialNumber );
    QAuthenticator cameraAuth;

    if ( existingRes )
    {
        cameraMAC = existingRes->getMAC();

        auto existAuth = existingRes->getAuth();
        cameraAuth = existAuth;
    }
    else
    {
        auto resData = qnCommon->dataPool()->data(manufacture(), model);
        auto possibleCreds = resData.value<DefaultCredentialsList>(
            Qn::POSSIBLE_DEFAULT_CREDENTIALS_PARAM_NAME);

        cameraAuth.setUser(kDefaultIsdUsername);
        cameraAuth.setPassword(kDefaultIsdPassword);

        for (const auto& creds: possibleCreds)
        {
            QAuthenticator auth = creds.toAuthenticator();
            QUrl url(lit("//") + deviceEndpoint.address.toString());
            if (testCredentials(url, auth))
            {
                cameraAuth = auth;
                break;
            }
        }
    }

	{
		QnMutexLocker lock(&m_mutex);

		if (m_alreadFoundMacAddresses.find(cameraMAC.toString()) == m_alreadFoundMacAddresses.end())
		{
			m_alreadFoundMacAddresses.insert(cameraMAC.toString());
			createResource( devInfo, cameraMAC, cameraAuth, m_foundUpnpResources);
		}
	}

	return true;
}

void QnPlISDResourceSearcher::createResource(
    const nx_upnp::DeviceInfo& devInfo,
    const QnMacAddress& mac,
    const QAuthenticator& auth,
    QnResourceList& result )
{

    QnUuid rt = qnResTypePool->getResourceTypeId(manufacture(), devInfo.modelName);
    if (rt.isNull())
    {
        rt = qnResTypePool->getResourceTypeId(manufacture(), kIsdDefaultResType);
        if (rt.isNull())
            return;
    }

    QnResourceData resourceData = qnCommon->dataPool()->data(devInfo.manufacturer, devInfo.modelName);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return;

    auto isDW = resourceData.value<bool>(Qn::DW_REBRANDED_TO_ISD_MODEL);
    auto vendor = isDW ? kDwFullVendorName :
        (devInfo.manufacturer == lit("ISD") || devInfo.manufacturer == kIsdFullVendorName) ?
            manufacture() :
            devInfo.manufacturer;

    auto name = (vendor == manufacture()) ?
        lit("ISD-") + devInfo.modelName :
        devInfo.modelName;

    QnPlIsdResourcePtr resource( new QnPlIsdResource() );

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
}

bool QnPlISDResourceSearcher::testCredentials(const QUrl &url, const QAuthenticator &auth)
{

    const auto host = url.host();
    const auto port = url.port(nx_http::DEFAULT_HTTP_PORT);

    if (host.isEmpty())
        return false;

    CLHttpStatus status = CLHttpStatus::CL_HTTP_AUTH_REQUIRED;
    auto data = downloadFile(
        status,
        kIsdModelInfoUrl,
        host,
        port,
        kDefaultIsdHttpTimeout,
        auth);

    return status == CLHttpStatus::CL_HTTP_SUCCESS;
}

#endif // #ifdef ENABLE_ISD
