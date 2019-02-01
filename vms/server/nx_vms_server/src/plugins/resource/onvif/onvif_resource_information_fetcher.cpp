#ifdef ENABLE_ONVIF

#include "onvif_resource_information_fetcher.h"

#include <memory>
#include <mutex>

#include <camera_vendors.h>
#include <onvif/soapDeviceBindingProxy.h>
#include <plugins/resource/onvif/onvif_resource.h>
#include <plugins/resource/onvif/onvif_searcher_hooks.h>
#include <plugins/resource/digitalwatchdog/digital_watchdog_resource.h>
#include <plugins/resource/archive_camera/archive_camera.h>
#include <plugins/resource/sony/sony_resource.h>
#include <plugins/resource/flex_watch/flexwatch_resource.h>
#include <plugins/resource/axis/axis_onvif_resource.h>
#include <plugins/resource/avigilon/avigilon_resource.h>
#include <plugins/resource/pelco/optera/optera_resource.h>
#include <plugins/resource/flir/flir_onvif_resource.h>
#include <plugins/resource/vista/vista_resource.h>
#include <plugins/resource/hikvision/hikvision_resource.h>
#include <plugins/resource/flir/flir_onvif_resource.h>
#include <plugins/resource/vivotek/vivotek_resource.h>
#include <plugins/resource/lilin/lilin_resource.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <nx/utils/log/log.h>

using namespace nx::plugins;
using namespace nx::plugins::onvif;
using namespace nx::vms::server::plugins;

const char* OnvifResourceInformationFetcher::ONVIF_RT = "ONVIF";
const char* ONVIF_ANALOG_RT = "ONVIF_ANALOG";

// Add vendor and camera model to omit ONVIF search (you have to add in case insensitive here)
static const char* ANALOG_CAMERAS[][2] =
{
    {"vivo_ironman", "VS8801"},
    {"VIVOTEK", "VS8801"}
};

// Add vendor and camera model to omit ONVIF search (case insensitive)
static const char* IGNORE_VENDORS[][2] =
{
    {"IP*", "*networkcamera*"}, // DLINK
    {"ISD", "*"},              // ISD
    {"spartan-6*", "*"},       // ArecontVision
    {"Arecont Vision*", "*"},  // ArecontVision
    {"acti*", "*"},       // ACTi. Current ONVIF implementation quite unstable. Vendor name is not filled by camera!
    {"*", "KCM*"},        // ACTi
    {"*", "DWCA-*"},      // NEW ISD cameras rebranded to DW
    {"*", "DWEA-*"},      // NEW ISD cameras rebranded to DW
    {"*", "DWCS-*"},       // NEW ISD cameras rebranded to DW
    {"Network Optix", "*"}, // Nx cameras
    {"Digital Watchdog", "XPM-FL72-48MP"}, //For some reasons we want to use ISD resource instead Onvif Digital Watchdog one.
    {"Network Optix", "*"}, // Nx Cameras
    #if defined(ENABLE_HANWHA)
        {"Hanwha Techwin*", "*" },
        {"Samsung Techwin*", "*" },
    #endif
};

bool OnvifResourceInformationFetcher::isAnalogOnvifResource(const QString& vendor, const QString& model)
{
    for (uint i = 0; i < sizeof(ANALOG_CAMERAS)/sizeof(ANALOG_CAMERAS[0]); ++i)
    {
        QString vendorAnalog = QLatin1String(ANALOG_CAMERAS[i][0]);
        if ((vendor.compare(vendorAnalog, Qt::CaseInsensitive) == 0 || vendorAnalog == lit("*")) &&
            model.compare(QString(QLatin1String(ANALOG_CAMERAS[i][1])), Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

bool OnvifResourceInformationFetcher::isModelContainVendor(const QString& vendor, const QString& model)
{
    if (model.toLower().contains(vendor.toLower()))
        return true;
    else if (model.contains(L'-'))
        return true;
    else
        return false;
}

OnvifResourceInformationFetcher::OnvifResourceInformationFetcher(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule),
    camersNamesData(NameHelper::instance()),
    m_shouldStop(false)
{
    QnResourceTypePtr typePtr(qnResTypePool->getResourceTypeByName(QLatin1String(ONVIF_RT)));
    if (!typePtr.isNull()) {
        onvifTypeId = typePtr->getId();
    } else {
        qCritical() << "Can't find " << ONVIF_RT << " resource type in resource type pool";
    }

    typePtr = QnResourceTypePtr (qnResTypePool->getResourceTypeByName(QLatin1String(ONVIF_ANALOG_RT)));
    if (!typePtr.isNull()) {
        onvifAnalogTypeId = typePtr->getId();
    } else {
        qCritical() << "Can't find " << ONVIF_ANALOG_RT << " resource type in resource type pool";
    }

    m_hookChain.registerHook(searcher_hooks::commonHooks);
    m_hookChain.registerHook(searcher_hooks::hikvisionManufacturerReplacement);
    m_hookChain.registerHook(searcher_hooks::manufacturerReplacementByModel);
    m_hookChain.registerHook(searcher_hooks::pelcoModelNormalization);
    m_hookChain.registerHook(searcher_hooks::forcedAdditionalManufacturer);
    m_hookChain.registerHook(searcher_hooks::additionalManufacturerNormalization);
    m_hookChain.registerHook(searcher_hooks::swapVendorAndModel);
}

void OnvifResourceInformationFetcher::findResources(const EndpointInfoHash& endpointInfo, QnResourceList& result, DiscoveryMode discoveryMode) const
{
    EndpointInfoHash::ConstIterator iter = endpointInfo.begin();

    while(iter != endpointInfo.end() && !m_shouldStop) {
        findResources(iter.key(), iter.value(), result, discoveryMode);

        ++iter;
    }
}

bool OnvifResourceInformationFetcher::ignoreCamera(
    QnResourceDataPool* dataPool,
    const QString& manufacturer, const QString& name)
{
    QnResourceData resourceData = dataPool->data(manufacturer, name);

    if (resourceData.value<bool>(ResourceDataKey::kIgnoreONVIF))
        return true;

    for (uint i = 0; i < sizeof(IGNORE_VENDORS)/sizeof(IGNORE_VENDORS[0]); ++i)
    {
        QRegExp rxVendor(QLatin1String((const char*)IGNORE_VENDORS[i][0]), Qt::CaseInsensitive, QRegExp::Wildcard);
        QRegExp rxName(QLatin1String((const char*)IGNORE_VENDORS[i][1]), Qt::CaseInsensitive, QRegExp::Wildcard);

        if (rxVendor.exactMatch(manufacturer) && rxName.exactMatch(name))
            return true;
    }
    return false;
}

bool OnvifResourceInformationFetcher::isModelSupported(const QString& manufacturer, const QString& modelName)
{
    return NameHelper::instance().isManufacturerSupported(manufacturer) && NameHelper::instance().isSupported(modelName);
}

bool OnvifResourceInformationFetcher::needIgnoreCamera(
    QnResourceDataPool* dataPool,
    const QString& host,
    const QString& manufacturer,
    const QString& model)
{
    const bool forceOnvif = QnPlOnvifResource::isCameraForcedToOnvif(
        dataPool, manufacturer, model);
    if (forceOnvif)
        return false;

    auto normilizedModel = QString(model).replace(manufacturer, QString()).trimmed();
    if (NameHelper::instance().isManufacturerSupported(manufacturer) &&
        NameHelper::instance().isSupported(normilizedModel))
    {
        NX_DEBUG(typeid(OnvifResourceInformationFetcher), lit("ignore ONVIF camera %1 (%2-%3) because it supported by native driver").
            arg(host).
            arg(manufacturer).
            arg(model));
        return true;
    }
    if (ignoreCamera(dataPool, manufacturer, normilizedModel))
    {
        NX_DEBUG(typeid(OnvifResourceInformationFetcher), lit("ignore ONVIF camera %1 (%2-%3) because it is in the special blocking list").
            arg(host).
            arg(manufacturer).
            arg(model));
        return true;
    }
    return false;
}

void OnvifResourceInformationFetcher::findResources(
    const QString& endpoint,
    const EndpointAdditionalInfo& originalInfo,
    QnResourceList& result,
    DiscoveryMode discoveryMode) const
{
    if (endpoint.isEmpty()) {
        qDebug() << "OnvifResourceInformationFetcher::findResources: response packet was received, but appropriate URL was not found.";
        return;
    }

    auto info = originalInfo;
    m_hookChain.applyHooks(serverModule()->commonModule()->resourceDataPool(), &info);

    QString mac = info.mac;
    if (isMacAlreadyExists(info.uniqId, result) || isMacAlreadyExists(mac, result)) {
        return;
    }

    //if (info.name.contains(QLatin1String("netw")) || info.manufacturer.contains(QLatin1String("netw")))
    //    int n = 0;
    auto dataPool = serverModule()->commonModule()->resourceDataPool();
    if (needIgnoreCamera(dataPool, QUrl(endpoint).host(), info.manufacturer, info.name))
        return;

    QString manufacturer = info.manufacturer;
    QString model = info.name;
    QString firmware;
    QHostAddress sender(QUrl(endpoint).host());
    // TODO: #vasilenko UTF unuse std::string
    DeviceSoapWrapper soapWrapper(
        SoapTimeouts(serverModule()->settings().onvifTimeouts()),
        endpoint.toStdString(), QString(), QString(), 0);

    QnVirtualCameraResourcePtr existResource = resourcePool()->getNetResourceByPhysicalId(info.uniqId).dynamicCast<QnVirtualCameraResource>();

    if (existResource)
    {
        QAuthenticator auth = existResource->getAuth();

        if (!auth.isNull())
        {
            soapWrapper.setLogin(auth.user());
            soapWrapper.setPassword(auth.password());
        }
    }
    else if (!info.defaultLogin.isEmpty()) {
        soapWrapper.setLogin(info.defaultLogin);
        soapWrapper.setPassword(info.defaultPassword);
    }
    else if (discoveryMode != DiscoveryMode::partiallyEnabled)
        soapWrapper.fetchLoginPassword(serverModule()->commonModule(), info.manufacturer, info.name);

    if (!existResource && discoveryMode == DiscoveryMode::partiallyEnabled)
        return; //ignoring unknown cameras

    if (m_shouldStop)
        return;

    bool isAuthorized = false;
    if (existResource)
    {
        if (model.isEmpty())
            model = existResource->getModel();
        if (manufacturer.isEmpty())
            manufacturer = existResource->getVendor();
        if (mac.isEmpty())
            mac = existResource->getMAC().toString();
        if (firmware.isEmpty())
            firmware = existResource->getFirmware();
        if (existResource->getStatus() != Qn::Unauthorized)
            isAuthorized = true;
    }

    if (model.isEmpty() || manufacturer.isEmpty() ||
        // Optional fields are to be updated only if the camera is authorized, to prevent brute force
        // attacks for unauthorized cameras (Hikvision blocks after several attempts).
        (isAuthorized && (firmware.isEmpty() || nx::utils::MacAddress(mac).isNull())))
    {
        OnvifResExtInfo extInfo;
        QAuthenticator auth;
        auth.setUser(soapWrapper.login());
        auth.setPassword(soapWrapper.password());
        CameraDiagnostics::Result result = QnPlOnvifResource::readDeviceInformation(
            serverModule()->commonModule(),
            SoapTimeouts(serverModule()->settings().onvifTimeouts()), endpoint, auth, INT_MAX, &extInfo);

        if (m_shouldStop)
            return;

        if (!result && result.errorCode != CameraDiagnostics::ErrorCode::notAuthorised)
            return; // non onvif device

        if (!extInfo.vendor.isEmpty())
            manufacturer = extInfo.vendor;

        if (!extInfo.model.isEmpty())
            model = extInfo.model;
        if (!extInfo.firmware.isEmpty())
            firmware = extInfo.firmware;
        if (!extInfo.mac.isEmpty())
            mac = extInfo.mac;

        auto dataPool = serverModule()->commonModule()->resourceDataPool();
        if (needIgnoreCamera(dataPool, QUrl(endpoint).host(), manufacturer, model))
            return;
    }

    if (model.isEmpty()) {
        qWarning() << "OnvifResourceInformationFetcher::findResources: can't fetch name of ONVIF device: endpoint: " << endpoint << ", UniqueId: " << info.uniqId;
        model = info.uniqId;
    }

    QnPlOnvifResourcePtr res = createResource(manufacturer, firmware, QHostAddress(sender), QHostAddress(info.discoveryIp),
                                              model, mac, info.uniqId, soapWrapper.login(), soapWrapper.password(), endpoint);
    if (res)
        result << res;
    else
        return;

    QnPlOnvifResourcePtr onvifRes = existResource.dynamicCast<QnPlOnvifResource>();

    // checking for multichannel encoders
    if(onvifRes && onvifRes->getMaxChannels() > 1)
    {
        QString groupName;
        QString groupId;

        if (onvifRes->getGroupId().isEmpty())
        {
            groupId = info.uniqId;
            groupName = onvifRes->getModel() + QLatin1String(" ") + onvifRes->getHostAddress();
        }
        else {
            groupId = onvifRes->getGroupId();
            groupName = onvifRes->getUserDefinedGroupName();
        }

        res->setGroupId(groupId);
        res->setDefaultGroupName(groupName);

        for (int i = 1; i < onvifRes->getMaxChannels(); ++i)
        {
            auto subres = createResource(manufacturer, firmware, QHostAddress(sender), QHostAddress(info.discoveryIp),
                model, mac, info.uniqId, soapWrapper.login(), soapWrapper.password(), endpoint);
            if (res) {
                QString suffix = QString(QLatin1String("?channel=%1")).arg(i+1);
                subres->setUrl(endpoint + suffix);
                subres->setPhysicalId(info.uniqId + suffix.replace(QLatin1String("?"), QLatin1String("_")));
                subres->setName(res->getName() + QString(QLatin1String("-channel %1")).arg(i+1));
                subres->setGroupId(groupId);
                subres->setDefaultGroupName(groupName);
                result << subres;
            }
        }
    }
}

QnUuid OnvifResourceInformationFetcher::getOnvifResourceType(const QString& manufacturer, const QString&  model) const
{
    const QString kOnvifManufacture("OnvifDevice");

    QnUuid rt = qnResTypePool->getResourceTypeId(kOnvifManufacture, manufacturer, false); // try to find child resource type, use real manufacturer name as camera model in onvif XML
    if (!rt.isNull())
        return rt;
    else if (isAnalogOnvifResource(manufacturer, model) && !onvifAnalogTypeId.isNull())
        return onvifAnalogTypeId;
    else
        return onvifTypeId; // no child resourceType found. Use root ONVIF resource type
}

QnPlOnvifResourcePtr OnvifResourceInformationFetcher::createResource(
    const QString& manufacturer, const QString& firmware, const QHostAddress& sender,
    const QHostAddress& discoveryIp, const QString& model,
    const QString& mac, const QString& uniqId, const QString& login, const QString& passwd,
    const QString& deviceUrl) const
{
    Q_UNUSED(discoveryIp)
    if (uniqId.isEmpty())
        return QnPlOnvifResourcePtr();

    auto resData = serverModule()->commonModule()->resourceDataPool()->data(manufacturer, model);
    auto manufacturerAlias = resData.value<QString>(ResourceDataKey::kOnvifVendorSubtype);

    manufacturerAlias = manufacturerAlias.isEmpty() ? manufacturer : manufacturerAlias;

    bool doNotAddVendorToDeviceName = resData.value<bool>(ResourceDataKey::kDoNotAddVendorToDeviceName);

    QnPlOnvifResourcePtr resource = createOnvifResourceByManufacture(serverModule(), manufacturerAlias);
    if (!resource)
        return resource;

    auto typeId = getOnvifResourceType(manufacturerAlias, model);
    NX_ASSERT(!typeId.isNull());
    resource->setTypeId(typeId);

    resource->setHostAddress(QHostAddress(sender).toString());
    resource->setModel(model);
    if ( isModelContainVendor(manufacturerAlias, model)
         || doNotAddVendorToDeviceName)
        resource->setName(model);
    else
        resource->setName(manufacturer + model);
    nx::utils::MacAddress macAddr(mac);
    resource->setMAC(macAddr);
    resource->setFirmware(firmware);

    resource->setPhysicalId(uniqId);
    resource->setUrl(deviceUrl);
    resource->setDeviceOnvifUrl(deviceUrl);

    if (!login.isEmpty())
        resource->setDefaultAuth(login, passwd);

    return resource;
}

bool OnvifResourceInformationFetcher::isMacAlreadyExists(const QString& mac, const QnResourceList& resList) const
{
    if (!mac.isEmpty()) {

        for(const QnResourcePtr& res: resList) {
            QnNetworkResourcePtr netRes = res.dynamicCast<QnNetworkResource>();

            if (netRes->getMAC().toString() == mac) {
                return true;
            }
        }
    }

    return false;
}

QString OnvifResourceInformationFetcher::fetchSerial(const DeviceInfoResp& response) const
{
    // TODO: #vasilenko UTF unuse std::string
    return response.HardwareId.empty()
        ? QString()
        : QString::fromStdString(response.HardwareId) + QLatin1String("::") +
            (response.SerialNumber.empty()
             ? QString()
             : QString::fromStdString(response.SerialNumber));
}

QnPlOnvifResourcePtr OnvifResourceInformationFetcher::createOnvifResourceByManufacture(
    QnMediaServerModule* serverModule,
    const QString& manufacture)
{
    QnPlOnvifResourcePtr resource;
    const QString lowerCaseManufacture = manufacture.toLower();

    if (lowerCaseManufacture.contains("digital watchdog")
        || lowerCaseManufacture.contains("digital_watchdog")
        || lowerCaseManufacture.contains("digitalwatchdog")
        || lowerCaseManufacture == "panoramic"
        || lowerCaseManufacture == "ipnc") //< new dw panoramic cameras
    {
        resource = QnPlOnvifResourcePtr(new QnDigitalWatchdogResource(serverModule));
    }
    else if (lowerCaseManufacture.contains(QLatin1String("sony")))
        resource = QnPlOnvifResourcePtr(new QnPlSonyResource(serverModule));
    else if (lowerCaseManufacture.contains(QLatin1String("seyeon tech")))
        resource = QnPlOnvifResourcePtr(new QnFlexWatchResource(serverModule));
    else if (lowerCaseManufacture.contains(QLatin1String("vista")))
        resource = QnPlOnvifResourcePtr(new QnVistaResource(serverModule));
    else if (lowerCaseManufacture.contains(QLatin1String("avigilon")))
        resource = QnPlOnvifResourcePtr(new QnAvigilonResource(serverModule));
    else if (lowerCaseManufacture.contains(QLatin1String("pelcooptera")))
        resource = QnPlOnvifResourcePtr(new QnOpteraResource(serverModule));
#ifdef ENABLE_AXIS
    else if (lowerCaseManufacture.contains(QLatin1String("axis")))
        resource = QnPlOnvifResourcePtr(new QnAxisOnvifResource(serverModule));
#endif
    else if (lowerCaseManufacture.contains(QLatin1String("hikvision")))
        resource = QnPlOnvifResourcePtr(new HikvisionResource(serverModule));
    else if (lowerCaseManufacture.contains(QLatin1String("flir")))
        resource = QnPlOnvifResourcePtr(new nx::plugins::flir::OnvifResource(serverModule));
    else if (lowerCaseManufacture.contains(QLatin1String("vivotek")))
        resource = QnPlOnvifResourcePtr(new VivotekResource(serverModule));
    else if (lowerCaseManufacture.contains(QLatin1String("merit-lilin")))
        resource = QnPlOnvifResourcePtr(new LilinResource(serverModule));
    else
        resource = QnPlOnvifResourcePtr(new QnPlOnvifResource(serverModule));

    resource->setVendor(manufacture);

    return resource;
}

void OnvifResourceInformationFetcher::pleaseStop()
{
    m_shouldStop = true;
}

#endif //ENABLE_ONVIF
