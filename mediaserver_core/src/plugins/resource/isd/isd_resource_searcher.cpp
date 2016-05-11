#ifdef ENABLE_ISD

#include <core/resource_management/resource_pool.h>
#include "core/resource/camera_resource.h"
#include "isd_resource_searcher.h"
#include "isd_resource.h"
#include <utils/network/http/httptypes.h>
#include "core/resource/resource_data.h"
#include "core/resource_management/resource_data_pool.h"
#include "common/common_module.h"


extern QString getValueFromString(const QString& line);

typedef QList<QMap<QString, QString>> DefaultCredentialsList;

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

    static const QLatin1String kDefaultIsdUsername( "root" );
    static const QLatin1String kDefaultIsdPassword( "admin" );
}

QnPlISDResourceSearcher::QnPlISDResourceSearcher()
{
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

        QMap<QString, QString> defaultCreds;
        defaultCreds["user"] = kDefaultIsdUsername;
        defaultCreds["password"] = kDefaultIsdPassword;
        possibleCreds << defaultCreds;

        for (const auto& creds: possibleCreds)
        {
            if ( creds["user"].isEmpty() || creds["password"].isEmpty())
                continue;

            QAuthenticator auth;
            auth.setUser(creds["user"]);
            auth.setPassword(creds["password"]);

            resList = checkHostAddrInternal(url, auth);
            if (!resList.isEmpty())
                break;
        }

        return resList;
    }

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
            name.replace(QLatin1Char(' '), QString());
            //name.replace(QLatin1Char('-'), QString());
            name.replace(QLatin1Char('\r'), QString());
            name.replace(QLatin1Char('\n'), QString());
            name.replace(QLatin1Char('\t'), QString());
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

    mac.replace(QLatin1Char(' '), QString());
    mac.replace(QLatin1Char('\r'), QString());
    mac.replace(QLatin1Char('\n'), QString());
    mac.replace(QLatin1Char('\t'), QString());

    if (mac.isEmpty() || name.isEmpty())
        return QList<QnResourcePtr>();


    mac = getValueFromString(mac).trimmed();

    //int n = mac.length();

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
    auto isDW = resourceData.value<bool>("isDW");

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

QString extractWord(int index, const QByteArray& rawData)
{
    int endIndex = index;
    for (;endIndex < rawData.size() && rawData.at(endIndex) != ' '; ++endIndex);
    return QString::fromLatin1(rawData.mid(index, endIndex - index));
}

/*QList<QnNetworkResourcePtr> QnPlISDResourceSearcher::processPacket(
    QnResourceList& result,
    const QByteArray& responseData,
    const QHostAddress& discoveryAddress,
    const QHostAddress& foundHostAddress )
{
    Q_UNUSED(discoveryAddress)
    Q_UNUSED(foundHostAddress)

    QList<QnNetworkResourcePtr> local_result;


    QString smac;
    QString name(lit("ISDcam"));

    if (!responseData.contains("ISD")) {
        // check for new ISD models. it has been rebrended
        int modelPos = responseData.indexOf("DWCA-");
        if (modelPos == -1)
            modelPos = responseData.indexOf("DWCS-");
		if (modelPos == -1)
			modelPos = responseData.indexOf("DWEA-");
        if (modelPos == -1)
            return local_result; // not found
        name = extractWord(modelPos, responseData);
    }

    int macpos = responseData.indexOf("macaddress=");
    if (macpos < 0)
        return local_result;

    macpos += QString(QLatin1String("macaddress=")).length();
    if (macpos + 12 > responseData.size())
        return local_result;

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
    
        if (net_res->getMAC().toString() == smac)
        {
            return local_result; // already found;
        }
    }


    QnPlIsdResourcePtr resource ( new QnPlIsdResource() );

    QnUuid rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (rt.isNull())
    {
        rt = qnResTypePool->getResourceTypeId(manufacture(), lit("ISDcam"));
        if (rt.isNull())
            return local_result;
    }

    QnResourceData resourceData = qnCommon->dataPool()->data(manufacture(), name);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return local_result; // model forced by ONVIF

    resource->setTypeId(rt);
    resource->setName(name);
    resource->setModel(name);
    resource->setMAC(QnMacAddress(smac));
    if (name == lit("DWcam"))
        resource->setDefaultAuth(QLatin1String("admin"), QLatin1String("admin"));
    local_result.push_back(resource);

    return local_result;


}*/

bool QnPlISDResourceSearcher::isDwOrIsd(const QString &vendorName) const
{
    return vendorName.toUpper().startsWith(manufacture()) ||
        vendorName.toLower().trimmed() == lit("digital watchdog") ||
        vendorName.toLower().trimmed() == lit("digitalwatchdog");
}

void QnPlISDResourceSearcher::processPacket(
        const QHostAddress& discoveryAddr,
        const HostAddress& host,
        const UpnpDeviceInfo& devInfo,
        const QByteArray& xmlDevInfo,
        QnResourceList& result )
{
    if (!isDwOrIsd(devInfo.manufacturer))
        return;

    QnMacAddress cameraMAC(devInfo.serialNumber);
    QString model(devInfo.modelName);
    QnNetworkResourcePtr existingRes = qnResPool->getResourceByMacAddress( devInfo.serialNumber );
    QAuthenticator cameraAuth;

    if ( existingRes )
    {
        cameraMAC = existingRes->getMAC();
        cameraAuth = existingRes->getAuth();
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
            QAuthenticator auth;
            auth.setUser(creds["user"]);
            auth.setPassword(creds["password"]);
            QUrl url(lit("//") + host.toString());
            if (testCredentials(url, auth))
            {
                cameraAuth = auth;
                break;
            }
        }
    }

    createResource( devInfo, cameraMAC, cameraAuth, result );
}

void QnPlISDResourceSearcher::createResource(
    const UpnpDeviceInfo& devInfo,
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

    QnResourceData resourceData = qnCommon->dataPool()->data(manufacture(), devInfo.modelName);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return;

    auto isDW = resourceData.value<bool>("isDW");
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
