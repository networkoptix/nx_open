#ifdef ENABLE_ACTI

#include "acti_resource_searcher.h"
#include "acti_resource.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>

#include <nx/network/deprecated/asynchttpclient.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>
#include <common/common_module.h>
#include <common/static_common_module.h>

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
const QString kSystemInfoModelParamName("model number");
const QString kSystemInfoMacParamName("mac address");
const QString kSystemInfoCompanyParamName("company name");
static const QString kUpnpBasicDeviceType("BASIC");

const int kActiDeviceXmlPort = 49152;
const int kDefaultActiTimeout = 4000;
const int kCacheExpirationInterval = 60 * 1000;
}

const QString QnActiResourceSearcher::kSystemInfoProductionIdParamName("production id");

QnActiResourceSearcher::QnActiResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule),
    QnAbstractNetworkResourceSearcher(commonModule),
    base_type(kUpnpBasicDeviceType)
{
    m_resTypeId = qnResTypePool->getResourceTypeId(manufacture(), QLatin1String("ACTI_COMMON"));
}

QnActiResourceSearcher::~QnActiResourceSearcher()
{
}

nx::network::upnp::DeviceInfo QnActiResourceSearcher::parseDeviceXml(
    const QByteArray& rawData,
    bool* outStatus) const
{
    *outStatus = true;
    nx::network::upnp::DeviceDescriptionHandler xmlHandler;
    QXmlSimpleReader xmlReader;
    xmlReader.setContentHandler( &xmlHandler );
    xmlReader.setErrorHandler( &xmlHandler );

    QXmlInputSource input;
    input.setData(rawData);
    if(!xmlReader.parse(&input))
    {
        *outStatus = false;
        return nx::network::upnp::DeviceInfo();
    }

    return xmlHandler.deviceInfo();
}

nx::network::upnp::DeviceInfo QnActiResourceSearcher::getDeviceInfoSync(const nx::utils::Url &url, bool* outStatus) const
{
    nx::network::upnp::DeviceInfo deviceInfo;
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


QList<QnResourcePtr> QnActiResourceSearcher::checkHostAddr(const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    if (!url.scheme().isEmpty() && doMultichannelCheck)
        return QList<QnResourcePtr>();

    QnResourceList result;
    auto actiRes = QnActiResourcePtr(new QnActiResource());

    nx::utils::Url urlCopy(
        lit("http://%1:%2")
            .arg(url.host())
            .arg(url.port(nx::network::http::DEFAULT_HTTP_PORT)));

    actiRes->setUrl(urlCopy.toString());
    actiRes->setDefaultAuth(auth);

    QnActiResourceSearcher::CachedDevInfo devInfo;
    {
        QnMutexLocker lock(&m_mutex);
        auto devInfo = m_cachedDevInfo.value(urlCopy.toString());
    }

    if (!devInfo.info.presentationUrl.isEmpty() && devInfo.timer.elapsed() < kCacheExpirationInterval)
    {
        createResource(devInfo.info, devInfo.mac, auth, result);
        return result;
    }

    auto report = getActiSystemInfo(actiRes);
    if (!report)
        return result;
    const auto kSerialNumber = report->value(kSystemInfoProductionIdParamName);
    const auto kMacAddress = report->value(kSystemInfoMacParamName);
    const auto kModel = report->value(kSystemInfoModelParamName);
    const auto kCompany = report->value(kSystemInfoCompanyParamName);

    devInfo.info.friendlyName = kCompany; //< Looks wrong.
    devInfo.info.manufacturer = kCompany;
    devInfo.info.modelName = retreiveModel(kModel, kSerialNumber);
    devInfo.info.serialNumber = kSerialNumber;
    devInfo.info.presentationUrl = actiRes->getUrl();
    devInfo.mac = nx::network::QnMacAddress(kMacAddress);

    if (devInfo.info.modelName.isEmpty()
        || (devInfo.info.serialNumber.isEmpty() && devInfo.mac.isNull()))
    {
        return result;
    }

    auto resourceData = qnStaticCommon->dataPool()->data(manufacture(), devInfo.info.modelName);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return result;

    devInfo.timer.restart();

    {
        QnMutexLocker lock(&m_mutex);
        m_cachedDevInfo[urlCopy.toString()] = devInfo;
    }

    createResource(devInfo.info, devInfo.mac, auth, result);
    return result;
}

boost::optional<QnActiResource::ActiSystemInfo> QnActiResourceSearcher::getActiSystemInfo(const QnActiResourcePtr& actiResource)
{
    CLHttpStatus status;
    auto response = actiResource->makeActiRequest(
        kActiSystemGroup,
        kActiSystemInfoCommand,
        status,
        true);

    if (shouldStop())
        return boost::none;

    if (status != CL_HTTP_SUCCESS)
    {
        bool upnpDevInfoStatus = false;
        nx::utils::Url deviceXmlUrl(actiResource->getUrl());
        deviceXmlUrl.setPort(kActiDeviceXmlPort);
        deviceXmlUrl.setPath(kActiDeviceXmlPath);

        auto upnpDevInfo = getDeviceInfoSync(deviceXmlUrl, &upnpDevInfoStatus);

        if (!upnpDevInfoStatus || shouldStop())
            return boost::none;

        actiResource->setUrl(upnpDevInfo.presentationUrl);
        response = actiResource->makeActiRequest(
            kActiSystemGroup,
            kActiSystemInfoCommand,
            status,
            true);

        if(status != CL_HTTP_SUCCESS || shouldStop())
            return boost::none;
    }

    auto report = QnActiResource::parseSystemInfo(response);

    if (report.isEmpty())
        return boost::none;

    return report;
}

QString QnActiResourceSearcher::retreiveModel(const QString& model, const QString& serialNumber) const
{
    if (!model.isEmpty())
        return model;

    return serialNumber.split(L'-')[0];
}

static QString stringToActiPhysicalID( const QString& serialNumber )
{
    QString sn = serialNumber;
    sn.replace( lit(":"), lit("_"));
    return QString(lit("ACTI_%1")).arg(sn);
}

QnResourceList QnActiResourceSearcher::findResources()
{
    QnMutexLocker lock(&m_mutex);
    QnResourceList result = std::move(m_foundUpnpResources);
    m_foundUpnpResources.clear();
    m_alreadFoundMacAddresses.clear();
    return result;
}

bool QnActiResourceSearcher::processPacket(
    const QHostAddress& /*discoveryAddr*/,
    const nx::network::SocketAddress& /*deviceEndpoint*/,
    const nx::network::upnp::DeviceInfo& devInfo,
    const QByteArray& /*xmlDevInfo*/)
{
    QnResourceList result;
    const bool isNx = isNxDevice(devInfo);
    if (!devInfo.manufacturer.toUpper().startsWith(manufacture()) && !isNx)
        return false;

    nx::network::upnp::DeviceInfo devInfoCopy(devInfo);
    nx::network::QnMacAddress cameraMac;

    if(isNx)
        devInfoCopy.friendlyName = NX_VENDOR;

    auto physicalId = stringToActiPhysicalID(devInfo.serialNumber);
    if (m_alreadFoundMacAddresses.contains(physicalId))
        return true;
    m_alreadFoundMacAddresses.insert(physicalId);

    auto existingRes = resourcePool()->getNetResourceByPhysicalId(physicalId);

    QAuthenticator cameraAuth;
    const QString defaultLogin = isNx ? DEFAULT_NX_LOGIN : DEFAULT_LOGIN;
    const QString defaultPassword = isNx ? DEFAULT_NX_PASSWORD : DEFAULT_PASSWORD;

    cameraAuth.setUser(defaultLogin);
    cameraAuth.setPassword(defaultPassword);

    if( existingRes )
    {
        // We use existing resource if there is one.
        cameraMac = existingRes->getMAC();
        cameraAuth = existingRes->getAuth();
        createResource(devInfoCopy, cameraMac, cameraAuth, result);
    }
    else
    {
        auto host = QUrl(devInfo.presentationUrl).host();

        if (host.isEmpty())
            return false; //< Not sure if it's right.

        if (!m_systemInfoCheckers.contains(host))
        {
            auto checker = std::make_shared<QnActiSystemInfoChecker>(nx::utils::Url(devInfo.presentationUrl));
            m_systemInfoCheckers[host] = checker;
        }


        // Possible auth = auth from resources with the same host address + default one.
        QSet<QAuthenticator> possibleAuth;
        auto sameHostResources = resourcePool()->getAllNetResourceByHostAddress(host)
            .filtered<QnActiResource>();

        if (!sameHostResources.isEmpty())
        {
            for (const auto& sameHostResource: sameHostResources)
                possibleAuth.insert(sameHostResource->getAuth());
        }

        possibleAuth.insert(cameraAuth);

        m_systemInfoCheckers[host]->setAuthOptions(possibleAuth);
        auto systemInfo = m_systemInfoCheckers[host]->getSystemInfo();

        if (!systemInfo)
        {
            // Create resource if we already checked all possible auth options .
            if (m_systemInfoCheckers[host]->isFailed())
                createResource(devInfoCopy, cameraMac, cameraAuth, result);

            return false;
        }

        // At this point we have system info.
        auto mac = systemInfo->value(lit("mac address"));
        if (!mac.isEmpty())
        {
            cameraMac = nx::network::QnMacAddress(mac);
            auto auth = m_systemInfoCheckers[host]->getSuccessfulAuth();
            if (!auth)
                return false;

            if(isNx)
            {
                devInfoCopy.friendlyName = NX_VENDOR;
                devInfoCopy.modelName = systemInfo->value("model number");
            }

            createResource(devInfoCopy, cameraMac, *auth, result);
        }
    }

    QnMutexLocker lock(&m_mutex);
    m_foundUpnpResources += result;

    return true;
}

void QnActiResourceSearcher::createResource(
    const nx::network::upnp::DeviceInfo& devInfo,
    const nx::network::QnMacAddress& mac,
    const QAuthenticator& auth,
    QnResourceList& result )
{
    if (m_resTypeId.isNull())
        return;

    const bool isNx = isNxDevice(devInfo);
    QnResourceData resourceData = qnStaticCommon->dataPool()->data(manufacture(), devInfo.modelName);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return;

    QnActiResourcePtr resource(new QnActiResource());
    resource->setTypeId(m_resTypeId);

    if(isNx)
    {
        resourceData = qnStaticCommon->dataPool()->data(NX_VENDOR, devInfo.modelName);
        auto name = resourceData.value<QString>(NX_DEVICE_NAME_PARAMETER_NAME);
        auto model = resourceData.value<QString>(NX_DEVICE_MODEL_PARAMETER_NAME);

        resource->setVendor(NX_VENDOR);
        resource->setName(name.isEmpty() ? devInfo.modelName : name);
        resource->setModel(model.isEmpty() ? devInfo.modelName : model);
    }
    else
    {
        resource->setName(lit("ACTi-%1").arg(devInfo.modelName));
        resource->setModel(devInfo.modelName);
    }

    resource->setUrl(QUrl(devInfo.presentationUrl).toString(QUrl::StripTrailingSlash));
    resource->setMAC(mac);
    resource->setPhysicalId(chooseProperPhysicalId(
        QUrl(devInfo.presentationUrl).host(),
        devInfo.serialNumber,
        mac.toString()));

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

bool QnActiResourceSearcher::isNxDevice(const nx::network::upnp::DeviceInfo& devInfo) const
{
    return devInfo.manufacturer.toLower().trimmed() == NX_VENDOR.toLower() ||
        devInfo.friendlyName.toLower().trimmed() == NX_VENDOR.toLower();
}

QString QnActiResourceSearcher::chooseProperPhysicalId(
    const QString& hostAddress,
    const QString& serialNumber,
    const QString& macAddress)
{
    auto existingRes = findExistingResource(hostAddress, serialNumber, macAddress);

    if (existingRes)
        return existingRes->getPhysicalId();

    return stringToActiPhysicalID(serialNumber);
}

QnNetworkResourcePtr QnActiResourceSearcher::findExistingResource(
    const QString& hostAddress,
    const QString& serialNumber,
    const QString& macAddress)
{
    auto existingRes = resourcePool()->getNetResourceByPhysicalId(stringToActiPhysicalID(serialNumber));

    if (!existingRes)
        existingRes = resourcePool()->getNetResourceByPhysicalId(stringToActiPhysicalID(macAddress));

    if (!existingRes && !nx::network::QnMacAddress(macAddress).isNull())
        existingRes = resourcePool()->getResourceByMacAddress(macAddress);

    if (!existingRes && !serialNumber.isEmpty())
    {
        auto sameHostResources = resourcePool()->getAllNetResourceByHostAddress(hostAddress)
            .filtered<QnActiResource>();

        for (const auto& camera: sameHostResources)
        {
            if (camera->getProperty(kSystemInfoProductionIdParamName) == serialNumber)
                return camera;
            // some cameras has different serial in multicast and via HTTP request. At this case multicast serial has last 6 digits of MAC address
            if (serialNumber.startsWith("CAMERA-"))
            {
                QString macSuffix = serialNumber.right(6);
                QString cameraMac = camera->getMAC().toString();
                cameraMac.replace("-", "");
                if (cameraMac.endsWith(macSuffix))
                    return camera;
            }
        }
    }

    return existingRes;
}

#endif // #ifdef ENABLE_ACTI
