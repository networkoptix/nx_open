#ifdef ENABLE_ACTI

#include <core/resource_management/resource_pool.h>

#include "acti_resource_searcher.h"
#include "core/resource/camera_resource.h"
#include "acti_resource.h"
#include "../mdns/mdns_listener.h"
#include <nx/network/http/asynchttpclient.h>
#include "core/resource/resource_data.h"
#include "core/resource_management/resource_data_pool.h"
#include "common/common_module.h"


extern QString getValueFromString(const QString& line);

static const QString DEFAULT_LOGIN(QLatin1String("admin"));
static const QString DEFAULT_PASSWORD(QLatin1String("123456"));
static const QString NX_VENDOR(QLatin1String("Network Optix"));
static const QString DEFAULT_NX_LOGIN(QLatin1String("admin"));
static const QString DEFAULT_NX_PASSWORD(QLatin1String("123456"));
static const QString NX_DEVICE_NAME_PARAMETER_NAME(QLatin1String("nxDeviceName"));
static const QString NX_DEVICE_MODEL_PARAMETER_NAME(QLatin1String("nxDeviceModel"));
static const int ACTI_DEVICEXML_PORT = 49152;
static const int CACHE_UPDATE_TIME = 60 * 1000;

QnActiResourceSearcher::QnActiResourceSearcher():
    QObject(), QnUpnpResourceSearcher()
{
    QnMdnsListener::instance()->registerConsumer((long) this);
    m_resTypeId = qnResTypePool->getResourceTypeId(manufacture(), QLatin1String("ACTI_COMMON"));
}

QnActiResourceSearcher::~QnActiResourceSearcher()
{
}

QnResourceList QnActiResourceSearcher::findResources(void)
{
    // find via pnp
    QnResourceList result = QnUpnpResourceSearcher::findResources();

    // find via MDNS
    QList<QByteArray> processedUuid;
    QnMdnsListener::ConsumerDataList data = QnMdnsListener::instance()->getData((long) this);
    for (int i = 0; i < data.size(); ++i)
    {
        if (shouldStop())
            break;

        QString removeAddress = data[i].remoteAddress;
        QByteArray uuidStr("ACTI");
        uuidStr += data[i].remoteAddress.toUtf8();
        QByteArray response = data[i].response;
        if (response.contains("_http") && response.contains("_tcp") && response.contains("local"))
        {
            if (processedUuid.contains(uuidStr))
                continue;
            if (response.contains("AXIS"))
                continue;
            QByteArray response = getDeviceXml(
                    QString(QLatin1String("http://%1:%2/devicedesc.xml"))
                        .arg(removeAddress)
                        .arg(ACTI_DEVICEXML_PORT)); // async request
            //QByteArray response = getDeviceXml(QString(QLatin1String("http://%1:%2")).arg(removeAddress).arg(80)); // test request
            processDeviceXml(response, removeAddress, removeAddress, result);
            processedUuid << uuidStr;
        }
    }
    
    return result;
}

QByteArray QnActiResourceSearcher::getDeviceXml(const QUrl& url)
{
    QnMutexLocker lock( &m_mutex );

    QString host = url.host();
    CasheInfo info = m_cachedXml.value(host);
    if (!m_cachedXml.contains(host) || info.timer.elapsed() > CACHE_UPDATE_TIME)
    {
        if (!m_httpInProgress.contains(url.host())) 
        {
            QString urlStr = url.toString();

            auto request = nx_http::AsyncHttpClient::create();
            connect(
                request.get(), &nx_http::AsyncHttpClient::done,
                this, &QnActiResourceSearcher::at_httpConnectionDone,
                Qt::DirectConnection );
            request->doGet(url);
            m_httpInProgress[url.host()] = request;
        }
    }

    return m_cachedXml.value(host).xml;
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
    {
        //qDebug() << "Manufature " << resourceType->getManufacture() << " != " << manufacture();
        return result;
    }

    result = QnVirtualCameraResourcePtr( new QnActiResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create ACTI camera resource. TypeID" << resourceTypeId.toString(); // << ", Parameters: " << parameters;
    //result->deserialize(parameters);

    return result;

}

QString QnActiResourceSearcher::manufacture() const
{
    return QnActiResource::MANUFACTURE;
}


QList<QnResourcePtr> QnActiResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    if( !url.scheme().isEmpty() && doMultichannelCheck )
        return QList<QnResourcePtr>();  //searching if only host is present, not specific protocol

    Q_UNUSED(doMultichannelCheck)

    QnResourceList result;

    QnActiResourcePtr actiRes(new QnActiResource);
    actiRes->setUrl(url.toString());
    actiRes->setDefaultAuth(auth);

    QString devUrl = QString(lit("http://%1:%2")).arg(url.host()).arg(url.port(80));
    CashedDevInfo devInfo = m_cashedDevInfo.value(devUrl);

    if (devInfo.info.presentationUrl.isEmpty() || devInfo.timer.elapsed() > CACHE_UPDATE_TIME)
    {
        QnMacAddress cameraMAC;
        CLHttpStatus status;
        QByteArray serverReport = actiRes->makeActiRequest(QLatin1String("system"), QLatin1String("SYSTEM_INFO"), status, true);
        if (status != CL_HTTP_SUCCESS)
            return result;
        QMap<QByteArray, QByteArray> report = QnActiResource::parseSystemInfo(serverReport);
        if (report.isEmpty())
            return result;

        devInfo.info.presentationUrl = devUrl;
        devInfo.info.friendlyName = QString::fromUtf8(report.value("company name"));
        devInfo.info.manufacturer = manufacture();
        QByteArray model = report.value("production id").split('-')[0];
        devInfo.info.modelName = QString::fromUtf8(QnActiResource::unquoteStr(model));
        cameraMAC = QnMacAddress(report.value("mac address"));
        devInfo.info.serialNumber = cameraMAC.toString();

        if (devInfo.info.modelName.isEmpty() || devInfo.info.serialNumber.isEmpty())
            return result;

        QnResourceData resourceData = qnCommon->dataPool()->data(manufacture(), QLatin1String(model));
        if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
            return result; // model forced by ONVIF


        devInfo.timer.restart();
        m_cashedDevInfo[devUrl] = devInfo;
    }
    createResource( devInfo.info, QnMacAddress(devInfo.info.serialNumber), auth, result );

    return result;
}

static QString serialNumberToPhysicalID( const QString& serialNumber )
{
    QString sn = serialNumber;
    sn.replace( lit(":"), lit("_"));
    return QString(lit("ACTI_%1")).arg(sn);
}

void QnActiResourceSearcher::processPacket(
    const QHostAddress& /*discoveryAddr*/,
    const HostAddress& /*host*/,
    const nx_upnp::DeviceInfo& devInfo,
    const QByteArray& /*xmlDevInfo*/,
    const QAuthenticator& auth,
    QnResourceList& result )
{

    const bool isNx = isNxDevice(devInfo);
    if (!devInfo.manufacturer.toUpper().startsWith(manufacture()) && !isNx)
        return;

    nx_upnp::DeviceInfo realDevInfo(devInfo);
    QnMacAddress cameraMAC;
    QnNetworkResourcePtr existingRes = qnResPool->getNetResourceByPhysicalId( serialNumberToPhysicalID(devInfo.serialNumber) );
    QAuthenticator cameraAuth;
    const QString defaultLogin = isNx ? DEFAULT_NX_LOGIN : DEFAULT_LOGIN;
    const QString defaultPassword = isNx ? DEFAULT_NX_PASSWORD : DEFAULT_LOGIN;

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
            QUrl(devInfo.presentationUrl), cameraAuth, lit("system"), lit("SYSTEM_INFO"), true, &serverReport );
        if( status == CL_HTTP_SUCCESS )
        {
            QMap<QByteArray, QByteArray> report = QnActiResource::parseSystemInfo(serverReport);
            cameraMAC = QnMacAddress(report.value("mac address"));
            if(isNx)
            {
                realDevInfo.friendlyName = NX_VENDOR;
                realDevInfo.modelName = report.value("model number");
            }
        }
    }

    createResource( realDevInfo, cameraMAC, auth, result );
}

void QnActiResourceSearcher::createResource(
    const nx_upnp::DeviceInfo& devInfo,
    const QnMacAddress& mac,
    const QAuthenticator& auth,
    QnResourceList& result )
{
    if (m_resTypeId.isNull())
        return;

    const bool isNx = isNxDevice(devInfo);

    QnResourceData resourceData = qnCommon->dataPool()->data(manufacture(), devInfo.modelName);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return; // model forced by ONVIF

    QnActiResourcePtr resource( new QnActiResource() );    

    resource->setTypeId(m_resTypeId);

    if(isNx)
    {
        auto data = getNxDataByActiModel(devInfo.modelName);
        resource->setVendor(NX_VENDOR);
        resource->setName(data["name"]);
        resource->setModel(data["model"]);
    }
    else
    {
        resource->setName(QString(QLatin1String("ACTi-")) + devInfo.modelName);
        resource->setModel(devInfo.modelName);
    }

    resource->setUrl(devInfo.presentationUrl);
    resource->setMAC(mac);
    resource->setPhysicalId( serialNumberToPhysicalID(devInfo.serialNumber) );

    if (!auth.isNull()) {
        resource->setDefaultAuth(auth);
    } else {
        QAuthenticator defaultAuth;
        const QString defaultLogin = isNx ? DEFAULT_NX_LOGIN : DEFAULT_LOGIN;
        const QString defaultPassword = isNx ? DEFAULT_NX_PASSWORD : DEFAULT_PASSWORD;
        defaultAuth.setUser(defaultLogin);
        defaultAuth.setPassword(defaultPassword);
        resource->setDefaultAuth(defaultAuth);
    }

    result << resource;
}

QMap<QString, QString> QnActiResourceSearcher::getNxDataByActiModel(const QString &model) const
{
    QMap<QString, QString> data;
    QnResourceData resourceData = qnCommon->dataPool()->data(manufacture(), model);

    data["name"] = resourceData.value<QString>(NX_DEVICE_NAME_PARAMETER_NAME);
    data["model"] = resourceData.value<QString>(NX_DEVICE_MODEL_PARAMETER_NAME);

    return data;
}

bool QnActiResourceSearcher::isNxDevice(const nx_upnp::DeviceInfo& devInfo) const
{
    return devInfo.manufacturer.toLower().trimmed() == NX_VENDOR.toLower() ||
        devInfo.friendlyName.toLower().trimmed() == NX_VENDOR.toLower();
}

#endif // #ifdef ENABLE_ACTI
