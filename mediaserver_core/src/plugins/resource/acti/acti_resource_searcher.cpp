#ifdef ENABLE_ACTI

#include "acti_resource_searcher.h"
#include "acti_resource.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>

#include <plugins/resource/mdns/mdns_listener.h>
#include <utils/network/http/asynchttpclient.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>
#include <common/common_module.h>
#include <utils/common/log.h>


extern QString getValueFromString(const QString& line);

namespace
{
const QString DEFAULT_LOGIN(QLatin1String("admin"));
const QString DEFAULT_PASSWORD(QLatin1String("123456"));
const QString NX_VENDOR(QLatin1String("Network Optix"));
const QString DEFAULT_NX_LOGIN(QLatin1String("admin"));
const QString DEFAULT_NX_PASSWORD(QLatin1String("nxwitness"));
const QString NX_DEVICE_NAME_PARAMETER_NAME(QLatin1String("nxDeviceName"));
const QString NX_DEVICE_MODEL_PARAMETER_NAME(QLatin1String("nxDeviceModel"));

const QString kActiSystemGroup("system");
const QString kActiSystemInfoCommand("SYSTEM_INFO");
const QString kActiDeviceXmlPath("devicedesc.xml");
const QString kSystemInfoProductionIdParamName("production id");
const QString kSystemInfoModelParamName("model number");
const QString kSystemInfoMacParamName("mac address");
const QString kSystemInfoCompanyParamName("company name");

const int kActiDeviceXmlPort = 49152;
const int kDefaultActiTimeout = 4000;
const int kCacheExpirationInterval = 60 * 1000;
}

QnActiResourceSearcher::QnActiResourceSearcher()
{
    QnMdnsListener::instance()->registerConsumer((std::uintptr_t) this);
    m_resTypeId = qnResTypePool->getResourceTypeId(manufacture(), QLatin1String("ACTI_COMMON"));
}

QnActiResourceSearcher::~QnActiResourceSearcher()
{
}

QnResourceList QnActiResourceSearcher::findResources(void)
{
    QnResourceList result;
    QList<QByteArray> processedUuid;

    auto data = QnMdnsListener::instance()->getData((std::uintptr_t) this);
    for (int i = 0; i < data.size(); ++i)
    {
        if (shouldStop())
            break;

        QString remoteAddress = data[i].remoteAddress;
        QByteArray uuidStr("ACTI");
        uuidStr += data[i].remoteAddress.toUtf8();
        QByteArray response = data[i].response;

        if (response.contains("_http") && response.contains("_tcp") && response.contains("local"))
        {
            if (processedUuid.contains(uuidStr))
                continue;
            if (response.contains("AXIS"))
                continue;

            auto response = getDeviceXmlAsync(
                lit("http://%1:%2/%3")
                    .arg(remoteAddress)
                    .arg(kActiDeviceXmlPort)
                    .arg(kActiDeviceXmlPath));
            
            processDeviceXml(response, remoteAddress, remoteAddress, result);
            processedUuid << uuidStr;
        }
    }
    
    return result;
}

UpnpDeviceInfo QnActiResourceSearcher::parseDeviceXml(
    const QByteArray& rawData,
    bool* outStatus) const
{
    *outStatus = true;
    UpnpDeviceDescriptionSaxHandler xmlHandler;
    QXmlSimpleReader xmlReader;
    xmlReader.setContentHandler( &xmlHandler );
    xmlReader.setErrorHandler( &xmlHandler );

    QXmlInputSource input;
    input.setData(rawData);
    if(!xmlReader.parse(&input))
    {
        *outStatus = false;
        return UpnpDeviceInfo();
    }

    return xmlHandler.deviceInfo();
}

QByteArray QnActiResourceSearcher::getDeviceXmlAsync(const QUrl& url)
{
    QnMutexLocker lock( &m_mutex );

    auto host = url.host();
    auto info = m_cachedXml.value(host);
    if (!m_cachedXml.contains(host) || info.timer.elapsed() > kCacheExpirationInterval)
    {
        if (!m_httpInProgress.contains(url.host())) 
        {
            auto urlStr = url.toString();
            auto request = nx_http::AsyncHttpClient::create();

            connect(
                request.get(), &nx_http::AsyncHttpClient::done,
                this, &QnActiResourceSearcher::at_httpConnectionDone,
                Qt::DirectConnection );

            if( request->doGet(url) )
                m_httpInProgress[url.host()] = request;
        }
    }

    return m_cachedXml.value(host).xml;
}

UpnpDeviceInfo QnActiResourceSearcher::getDeviceInfoSync(const QUrl& url, bool* outStatus) const
{
    UpnpDeviceInfo deviceInfo;
    QByteArray response;
    CLSimpleHTTPClient client(
        url.host(),
        url.port(kActiDeviceXmlPort),
        kDefaultActiTimeout,
        QAuthenticator());

    CLHttpStatus status = client.doGET(kActiDeviceXmlPath);
    if (status != CL_HTTP_SUCCESS)
    {
        *outStatus = false;
        return deviceInfo;
    }
    
    client.readAll(response);
    deviceInfo = parseDeviceXml(response, outStatus);

    return deviceInfo;
}

void QnActiResourceSearcher::processDeviceXml(
    const QByteArray& foundDeviceDescription,
    const HostAddress& host,
    const HostAddress& sender,
    QnResourceList& result )
{
    bool status = false;
    auto deviceInfo = parseDeviceXml(foundDeviceDescription, &status);
    
    if (!status)
        return;

    processPacket(
        QHostAddress(sender.toString()),
        host,
        deviceInfo,
        foundDeviceDescription,
        result);
}

void QnActiResourceSearcher::at_httpConnectionDone(nx_http::AsyncHttpClientPtr reply)
{
    QnMutexLocker lock( &m_mutex );

    QString host = reply->url().host();
    m_cachedXml[host].xml = reply->fetchMessageBodyBuffer();
    m_cachedXml[host].timer.restart();
    m_httpInProgress.remove(host);
}

QnResourcePtr QnActiResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& /*params*/)
{
    QnNetworkResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
    {
        qDebug() << "No resource type for ID = " << resourceTypeId;
        return result;
    }

    if (resourceType->getManufacture() != manufacture())
        return result;

    result = QnVirtualCameraResourcePtr( new QnActiResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create ACTI camera resource. TypeID" << resourceTypeId.toString(); // << ", Parameters: " << parameters;

    return result;

}

QString QnActiResourceSearcher::manufacture() const
{
    return QnActiResource::MANUFACTURE;
}


QList<QnResourcePtr> QnActiResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    if( !url.scheme().isEmpty() && doMultichannelCheck )
        return QList<QnResourcePtr>();

    QnResourceList result;

    QnActiResourcePtr actiRes(new QnActiResource);
    actiRes->setUrl(url.toString());
    actiRes->setDefaultAuth(auth);

    auto devUrl = lit("http://%1:%2")
        .arg(url.host())
        .arg(url.port(nx_http::DEFAULT_HTTP_PORT));

    auto deviceXmlUrl =
        lit("http://%1:%2/%3")
            .arg(url.host())
            .arg(kActiDeviceXmlPort)
            .arg(kActiDeviceXmlPath);

    auto devInfo = m_cachedDevInfo.value(devUrl);

    if (devInfo.info.presentationUrl.isEmpty() || devInfo.timer.elapsed() > kCacheExpirationInterval)
    {
        CLHttpStatus status;

        NX_LOG(lit("ACTI searcher, doing request to %1").arg(devUrl) ,cl_logINFO)

        auto serverReport = actiRes->makeActiRequest(
            kActiSystemGroup,
            kActiSystemInfoCommand,
            status,
            true);

        if (status != CL_HTTP_SUCCESS)
        { 
            NX_LOG(lit("(2) ACTI searcher, doing request to devicedesc.xml %1").arg(devUrl) ,cl_logINFO);
            /* Trying to get appropriate port from the UPnP device description XML,
             * which is always located on the same port.
             */ 
            bool upnpDevInfoStatus = false;
            auto upnpDevInfo = getDeviceInfoSync(deviceXmlUrl, &upnpDevInfoStatus);

            if (!upnpDevInfoStatus)
            {
                NX_LOG(lit("No response for devicedesc.xml %1").arg(deviceXmlUrl) ,cl_logINFO);
                return result;
            }

            NX_LOG(lit("ACTI searcher, got xml %1").arg(deviceXmlUrl) ,cl_logINFO);

            actiRes->setUrl(upnpDevInfo.presentationUrl);
            serverReport = actiRes->makeActiRequest(
                kActiSystemGroup,
                kActiSystemInfoCommand,
                status,
                true);

            NX_LOG(lit("ACTI searcher, doing request to System INFO %1").arg(deviceXmlUrl) ,cl_logINFO);
            if(status != CL_HTTP_SUCCESS)
            {
                NX_LOG(lit("ACTI searcher, no sytem info retrieved %1").arg(deviceXmlUrl), cl_logINFO);
                return result;
            }
        }

        auto report = QnActiResource::parseSystemInfo(serverReport);
        if (report.isEmpty())
            return result;

        devInfo.info.presentationUrl = actiRes->getUrl();
        devInfo.info.friendlyName = report.value(kSystemInfoCompanyParamName);
        devInfo.info.manufacturer = manufacture();

        auto model = report.value(kSystemInfoModelParamName);
        auto productionId = report.value(kSystemInfoProductionIdParamName);

        if (model.isEmpty())
            model = productionId.split('-')[0];

        devInfo.info.modelName = QnActiResource::unquoteStr(model.toUtf8());

        devInfo.mac = QnMacAddress(report.value(kSystemInfoMacParamName));
        devInfo.info.serialNumber = productionId;

        bool notEnoughData = devInfo.info.modelName.isEmpty()
            || (devInfo.info.serialNumber.isEmpty() && devInfo.mac.isNull());

        if (notEnoughData)
            return result;

        auto resourceData = qnCommon->dataPool()->data(manufacture(), model);
        if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
            return result;

        devInfo.timer.restart();
        m_cachedDevInfo[devUrl] = devInfo;
    }

    NX_LOG(lit("ACTI SEARCHER, creating resource"), cl_logINFO);
    createResource(devInfo.info, devInfo.mac, auth, result);

    return result;
}

static QString stringToActiPhysicalID( const QString& serialNumber )
{
    QString sn = serialNumber;
    sn.replace( lit(":"), lit("_"));
    return QString(lit("ACTI_%1")).arg(sn);
}

void QnActiResourceSearcher::processPacket(
    const QHostAddress& /*discoveryAddr*/,
    const HostAddress& /*host*/,
    const UpnpDeviceInfo& devInfo,
    const QByteArray& /*xmlDevInfo*/,
    QnResourceList& result )
{
    const bool isNx = isNxDevice(devInfo);
    if (!devInfo.manufacturer.toUpper().startsWith(manufacture()) && !isNx)
        return;

    UpnpDeviceInfo devInfoCopy(devInfo);
    QnMacAddress cameraMAC;
    auto existingRes = qnResPool->getNetResourceByPhysicalId(
        stringToActiPhysicalID(devInfo.serialNumber));

    QAuthenticator cameraAuth;
    const QString defaultLogin = isNx ? DEFAULT_NX_LOGIN : DEFAULT_LOGIN;
    const QString defaultPassword = isNx ? DEFAULT_NX_PASSWORD : DEFAULT_PASSWORD;

    cameraAuth.setUser(defaultLogin);
    cameraAuth.setPassword(defaultPassword);

    if( existingRes )
    {
        cameraMAC = existingRes->getMAC();
        cameraAuth = existingRes->getAuth();
    }

    if( cameraMAC.isNull() || isNx )
    {
        QByteArray serverReport;
        CLHttpStatus status = QnActiResource::makeActiRequest(
            QUrl(devInfo.presentationUrl),
            cameraAuth,
            kActiSystemGroup,
            kActiSystemInfoCommand,
            true,
            &serverReport);

        if( status == CL_HTTP_SUCCESS )
        {
            auto report = QnActiResource::parseSystemInfo(serverReport);
            cameraMAC = QnMacAddress(report.value(kSystemInfoMacParamName));

            if(isNx)
            {
                devInfoCopy.friendlyName = NX_VENDOR;
                devInfoCopy.modelName = report.value("model number");
            }
        }
        else
        {
            return;
        }
    }

    createResource(devInfoCopy, cameraMAC, cameraAuth, result);
}

void QnActiResourceSearcher::createResource(
    const UpnpDeviceInfo& devInfo,
    const QnMacAddress& mac,
    const QAuthenticator& auth,
    QnResourceList& result )
{
    if (m_resTypeId.isNull())
        return;

    const bool isNx = isNxDevice(devInfo);

    QnResourceData resourceData = qnCommon->dataPool()->data(manufacture(), devInfo.modelName);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return;

    QnActiResourcePtr resource(new QnActiResource());    
    resource->setTypeId(m_resTypeId);

    if(isNx)
    {    
        resource->setVendor(NX_VENDOR);
        resource->setName(resourceData.value<QString>(NX_DEVICE_NAME_PARAMETER_NAME));
        resource->setModel(resourceData.value<QString>(NX_DEVICE_MODEL_PARAMETER_NAME));
    }
    else
    {
        resource->setName(lit("ACTi-%1").arg(devInfo.modelName));
        resource->setModel(devInfo.modelName);
    }

    resource->setUrl(devInfo.presentationUrl);
    resource->setMAC(mac);
    resource->setPhysicalId(chooseProperPhysicalId(devInfo.serialNumber, mac.toString()));

    if (!auth.isNull())
    {
        resource->setDefaultAuth(auth);
    }
    else
    {
        QAuthenticator defaultAuth;
        const QString defaultLogin = isNx ? DEFAULT_NX_LOGIN : DEFAULT_LOGIN;
        const QString defaultPassword = isNx ? DEFAULT_NX_PASSWORD : DEFAULT_PASSWORD;
        defaultAuth.setUser(defaultLogin);
        defaultAuth.setPassword(defaultPassword);
        resource->setDefaultAuth(defaultAuth);
    }

    result << resource;
}

bool QnActiResourceSearcher::isNxDevice(const UpnpDeviceInfo& devInfo) const
{
    return devInfo.manufacturer.toLower().trimmed() == NX_VENDOR.toLower() ||
        devInfo.friendlyName.toLower().trimmed() == NX_VENDOR.toLower();
}

QString QnActiResourceSearcher::chooseProperPhysicalId(const QString& serialNumber, const QString& macAddress)
{
    auto existingRes = findExistingResource(serialNumber, macAddress);

    if (existingRes)
        return existingRes->getPhysicalId();

    return stringToActiPhysicalID(macAddress);
}

QnNetworkResourcePtr QnActiResourceSearcher::findExistingResource(const QString& serialNumber, const QString& macAddress)
{
    auto existingRes = qnResPool->getNetResourceByPhysicalId(stringToActiPhysicalID(serialNumber));

    if (!existingRes)
        existingRes = qnResPool->getNetResourceByPhysicalId(stringToActiPhysicalID(macAddress));
    
    return existingRes;
}

#endif // #ifdef ENABLE_ACTI
