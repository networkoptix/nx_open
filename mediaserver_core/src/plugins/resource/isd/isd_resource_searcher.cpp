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

static const QLatin1String DEFAULT_ISD_USERNAME( "root" );
static const QLatin1String DEFAULT_ISD_PASSWORD( "admin" );

QList<QnResourcePtr> QnPlISDResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& authOriginal, bool isSearchAction)
{
    if( !url.scheme().isEmpty() && isSearchAction )
        return QList<QnResourcePtr>();  //searching if only host is present, not specific protocol

    QAuthenticator auth( authOriginal );

    if( auth.user().isEmpty() )
        auth.setUser( DEFAULT_ISD_USERNAME );
    if( auth.password().isEmpty() )
        auth.setPassword( DEFAULT_ISD_PASSWORD );


    QString host = url.host();
    int port = url.port( nx_http::DEFAULT_HTTP_PORT );
    if (host.isEmpty())
        host = url.toString(); // in case if url just host address without protocol and port

    int timeout = 2000;

    QString name;
    QString vendor;

    CLHttpStatus status;
    QByteArray data = downloadFile(status, QLatin1String("/api/param.cgi?req=General.Brand.CompanyName&req=General.Brand.ModelName"), host, port, timeout, auth);
    for (const QByteArray& line: data.split(L'\n'))
    {
        if (line.startsWith("General.Brand.ModelName")) {
            name = getValueFromString(QString::fromUtf8(line)).trimmed();
            name.replace(QLatin1Char(' '), QString()); // remove spaces
            //name.replace(QLatin1Char('-'), QString()); // remove spaces
            name.replace(QLatin1Char('\r'), QString()); // remove spaces
            name.replace(QLatin1Char('\n'), QString()); // remove spaces
            name.replace(QLatin1Char('\t'), QString()); // remove tabs
        }
        else if (line.startsWith("General.Brand.CompanyName")) {
            vendor = getValueFromString(QString::fromUtf8(line)).trimmed();
        }
    }
    
    // 'Digital Watchdog' (with space) is actually ISD cameras. Without space it's old DW cameras
    if (name.isEmpty() || (vendor != lit("Innovative Security Designs") && vendor != lit("Digital Watchdog")))
        return QList<QnResourcePtr>();


    QString mac = QString(QLatin1String(downloadFile(status, QLatin1String("/api/param.cgi?req=Network.1.MacAddress"), host, port, timeout, auth)));

    mac.replace(QLatin1Char(' '), QString()); // remove spaces
    mac.replace(QLatin1Char('\r'), QString()); // remove spaces
    mac.replace(QLatin1Char('\n'), QString()); // remove spaces
    mac.replace(QLatin1Char('\t'), QString()); // remove tabs


    if (mac.isEmpty() || name.isEmpty())
        return QList<QnResourcePtr>();


    mac = getValueFromString(mac).trimmed();

    //int n = mac.length();

    if (mac.length() > 17 && mac.endsWith(QLatin1Char('0')))
        mac.chop(mac.length() - 17);





    QnUuid rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (rt.isNull()) {
        rt = qnResTypePool->getResourceTypeId(manufacture(), lit("ISDcam"));
        if (rt.isNull())
            return QList<QnResourcePtr>();
    }

    QnResourceData resourceData = qnCommon->dataPool()->data(manufacture(), name);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return QList<QnResourcePtr>(); // model forced by ONVIF

    QnPlIsdResourcePtr resource ( new QnPlIsdResource() );
    auto isDW = resourceData.value<bool>("isDW");
    vendor = isDW ? lit("Digital Watchdog") : vendor;
    name = isDW ? name : lit("ISD-") + name;

    resource->setTypeId(rt);
    resource->setVendor(vendor);
    resource->setName(name);
    resource->setModel(name);
    resource->setMAC(QnMacAddress(mac));
    if (port == 80)
        resource->setHostAddress(host);
    else
        resource->setUrl(QString(lit("http://%1:%2")).arg(host).arg(port));
    resource->setDefaultAuth(auth);

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

void QnPlISDResourceSearcher::processPacket(
        const QHostAddress& discoveryAddr,
        const HostAddress& host,
        const UpnpDeviceInfo& devInfo,
        const QByteArray& xmlDevInfo,
        QnResourceList& result )
{

    QAuthenticator auth;
    auth.setUser(DEFAULT_ISD_USERNAME);
    auth.setPassword(DEFAULT_ISD_PASSWORD);

    if (!devInfo.manufacturer.toUpper().startsWith(manufacture()))
        return;

    QnMacAddress cameraMAC(devInfo.serialNumber);
    QnNetworkResourcePtr existingRes = qnResPool->getResourceByMacAddress( devInfo.serialNumber );
    QAuthenticator cameraAuth;
    cameraAuth.setUser(DEFAULT_ISD_USERNAME);
    cameraAuth.setPassword(DEFAULT_ISD_PASSWORD);
    if( existingRes )
    {
        cameraMAC = existingRes->getMAC();
        cameraAuth = existingRes->getAuth();
    }

    createResource( devInfo, cameraMAC, auth, result );
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
        rt = qnResTypePool->getResourceTypeId(manufacture(), lit("ISDcam"));
        if (rt.isNull())
            return;
    }

    QnResourceData resourceData = qnCommon->dataPool()->data(manufacture(), devInfo.modelName);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return; // model forced by ONVIF

    auto isDW = resourceData.value<bool>("isDW");
    auto vendor = isDW ? lit("Digital Watchdog") : lit("ISD");
    auto name = isDW ? devInfo.modelName : lit("ISD-") + devInfo.modelName;
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
        defaultAuth.setUser(DEFAULT_ISD_USERNAME);
        defaultAuth.setPassword(DEFAULT_ISD_PASSWORD);
        resource->setDefaultAuth(defaultAuth);
    }

    result << resource;
}

#endif // #ifdef ENABLE_ISD
