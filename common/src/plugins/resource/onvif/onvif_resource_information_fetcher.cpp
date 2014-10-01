
#ifdef ENABLE_ONVIF

#include "onvif_resource_information_fetcher.h"
#include "onvif_resource.h"
#include "onvif/soapDeviceBindingProxy.h"
#include "../digitalwatchdog/digital_watchdog_resource.h"
#include "../sony/sony_resource.h"
#include "core/resource_management/resource_pool.h"
#include "plugins/resource/flex_watch/flexwatch_resource.h"
#include "plugins/resource/axis/axis_onvif_resource.h"
#include "../vista/vista_resource.h"

const char* OnvifResourceInformationFetcher::ONVIF_RT = "ONVIF";
const char* ONVIF_ANALOG_RT = "ONVIF_ANALOG";


// Add vendor and camera model to ommit ONVIF search (you have to add in case insensitive here)
static const char* ANALOG_CAMERAS[][2] =
{
    {"AXIS", "Q7404"},
    {"vivo_ironman", "VS8801"},
    {"VIVOTEK", "VS8801"}
};

// Add vendor and camera model to ommit ONVIF search (case insensitive)
static const char* IGNORE_VENDORS[][2] =
{
    {"*networkcamera*", "IP*"}, // DLINK
    {"*", "*spartan-6*"},       // ArecontVision
    {"acti*", "*"}              // ACTi. Current ONVIF implementation quite unstable. Vendor name is not filled by camera!
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

OnvifResourceInformationFetcher::OnvifResourceInformationFetcher():
    /*passwordsData(PasswordHelper::instance()),*/
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
}

OnvifResourceInformationFetcher& OnvifResourceInformationFetcher::instance()
{
    static OnvifResourceInformationFetcher inst;
    return inst;
}

void OnvifResourceInformationFetcher::findResources(const EndpointInfoHash& endpointInfo, QnResourceList& result) const
{
    EndpointInfoHash::ConstIterator iter = endpointInfo.begin();

    while(iter != endpointInfo.end() && !m_shouldStop) {
        findResources(iter.key(), iter.value(), result);

        ++iter;
    }
}

bool OnvifResourceInformationFetcher::ignoreCamera(const QString& manufacturer, const QString& name)
{
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

void OnvifResourceInformationFetcher::findResources(const QString& endpoint, const EndpointAdditionalInfo& info, QnResourceList& result) const
{
    if (endpoint.isEmpty()) {
        qDebug() << "OnvifResourceInformationFetcher::findResources: response packet was received, but appropriate URL was not found.";
        return;
    }

    QString mac = info.mac;
    if (isMacAlreadyExists(info.uniqId, result) || isMacAlreadyExists(mac, result)) {
        return;
    }

    //if (info.name.contains(QLatin1String("netw")) || info.manufacturer.contains(QLatin1String("netw")))
    //    int n = 0;

    if (ignoreCamera(info.manufacturer, info.name))
        return;

    if (isModelSupported(info.manufacturer, info.name)) {
        //qDebug() << "OnvifResourceInformationFetcher::findResources: skipping camera " << info.name;
        return;
    }

    QString manufacturer = info.manufacturer;
    QString model = info.name;
    QString firmware;
    QHostAddress sender(QUrl(endpoint).host());
    //TODO: #vasilenko UTF unuse std::string
    DeviceSoapWrapper soapWrapper(endpoint.toStdString(), QString(), QString(), 0);

    QnVirtualCameraResourcePtr existResource = qnResPool->getNetResourceByPhysicalId(info.uniqId).dynamicCast<QnVirtualCameraResource>();

    if (existResource) {
        soapWrapper.setLogin(existResource->getAuth().user());
        soapWrapper.setPassword(existResource->getAuth().password());
    }
    else if (!info.defaultLogin.isEmpty()) {
        soapWrapper.setLogin(info.defaultLogin);
        soapWrapper.setPassword(info.defaultPassword);
    }
    else
        soapWrapper.fetchLoginPassword(info.manufacturer);

    //Trying to get name and manufacturer
    if (existResource)
    {
        if (model.isEmpty())
            model = existResource->getModel();
        if (manufacturer.isEmpty())
            manufacturer = existResource->getVendor();
        if (mac.isEmpty())
            mac = existResource->getMAC().toString();
    }

    if (model.isEmpty() || manufacturer.isEmpty()  || QnMacAddress(mac).isNull())
    {
        OnvifResExtInfo extInfo;
        QAuthenticator auth;
        auth.setUser(soapWrapper.getLogin());
        auth.setPassword(soapWrapper.getPassword());
        CameraDiagnostics::Result result = QnPlOnvifResource::readDeviceInformation(endpoint, auth, INT_MAX, &extInfo);
        
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
            
        if( (camersNamesData.isManufacturerSupported(manufacturer) && camersNamesData.isSupported(QString(model).replace(manufacturer, QString()))) ||
            ignoreCamera(manufacturer, model) )
        {
            qDebug() << "OnvifResourceInformationFetcher::findResources: (later step) skipping camera " << model;
            return;
        }
    }

    if (model.isEmpty()) {
        qWarning() << "OnvifResourceInformationFetcher::findResources: can't fetch name of ONVIF device: endpoint: " << endpoint << ", UniqueId: " << info.uniqId;
        model = info.uniqId;
    }


    QnPlOnvifResourcePtr res = createResource(manufacturer, firmware, QHostAddress(sender), QHostAddress(info.discoveryIp),
                                              model, mac, info.uniqId, soapWrapper.getLogin(), soapWrapper.getPassword(), endpoint);
    if (res)
        result << res;
    else
        return;

    // checking for multichannel encoders
    QnPlOnvifResourcePtr onvifRes = existResource.dynamicCast<QnPlOnvifResource>();
    if (onvifRes && onvifRes->getMaxChannels() > 1) 
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
            groupName = onvifRes->getGroupName();
        }

        res->setGroupId(groupId);
        res->setGroupName(groupName);

        for (int i = 1; i < onvifRes->getMaxChannels(); ++i) 
        {
            res = createResource(manufacturer, firmware, QHostAddress(sender), QHostAddress(info.discoveryIp),
                model, mac, info.uniqId, soapWrapper.getLogin(), soapWrapper.getPassword(), endpoint);
            if (res) {
                QString suffix = QString(QLatin1String("?channel=%1")).arg(i+1);
                res->setUrl(endpoint + suffix);
                res->setPhysicalId(info.uniqId + suffix.replace(QLatin1String("?"), QLatin1String("_")));
                res->setName(res->getName() + QString(QLatin1String("-channel %1")).arg(i+1));
                res->setGroupId(groupId);
                res->setGroupName(groupName);
                result << res;
            }
        }
    }
}

QnUuid OnvifResourceInformationFetcher::getOnvifResourceType(const QString& manufacturer, const QString&  model) const
{
    QnUuid rt = qnResTypePool->getResourceTypeId(QLatin1String("OnvifDevice"), manufacturer, false); // try to find child resource type, use real manufacturer name as camera model in onvif XML
    if (!rt.isNull())
        return rt;
    else if (isAnalogOnvifResource(manufacturer, model) && !onvifAnalogTypeId.isNull())
        return onvifAnalogTypeId;
    else 
        return onvifTypeId; // no child resourceType found. Use root ONVIF resource type
}

QnPlOnvifResourcePtr OnvifResourceInformationFetcher::createResource(const QString& manufacturer, const QString& firmware, const QHostAddress& sender, const QHostAddress& discoveryIp, const QString& model, 
    const QString& mac, const QString& uniqId, const QString& login, const QString& passwd, const QString& deviceUrl) const
{
    if (uniqId.isEmpty())
        return QnPlOnvifResourcePtr();

    QnPlOnvifResourcePtr resource = createOnvifResourceByManufacture(manufacturer);
    if (!resource)
        return resource;

    resource->setTypeId(getOnvifResourceType(manufacturer, model));

    resource->setHostAddress(QHostAddress(sender).toString(), QnDomainMemory);
    resource->setDiscoveryAddr(discoveryIp);
    resource->setModel(model);
    if (isModelContainVendor(manufacturer, model))
        resource->setName(model); 
    else
        resource->setName(manufacturer + model); 
    QnMacAddress macAddr(mac);
    resource->setMAC(macAddr);
    resource->setFirmware(firmware);

	resource->setPhysicalId(uniqId);

    resource->setDeviceOnvifUrl(deviceUrl);

    if (!login.isEmpty())
        resource->setAuth(login, passwd);

    return resource;
}

bool OnvifResourceInformationFetcher::isMacAlreadyExists(const QString& mac, const QnResourceList& resList) const
{
    if (!mac.isEmpty()) {

        foreach(QnResourcePtr res, resList) {
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
    //TODO: #vasilenko UTF unuse std::string
    return response.HardwareId.empty()
        ? QString()
        : QString::fromStdString(response.HardwareId) + QLatin1String("::") +
            (response.SerialNumber.empty()
             ? QString() 
             : QString::fromStdString(response.SerialNumber));
}

QnPlOnvifResourcePtr OnvifResourceInformationFetcher::createOnvifResourceByManufacture(const QString& manufacture)
{
    QnPlOnvifResourcePtr resource;
    if (manufacture.toLower().contains(QLatin1String("digital watchdog")))
        resource = QnPlOnvifResourcePtr(new QnPlWatchDogResource());
    else if (manufacture.toLower().contains(QLatin1String("sony")))
        resource = QnPlOnvifResourcePtr(new QnPlSonyResource());
    else if (manufacture.toLower().contains(QLatin1String("seyeon tech")))
        resource = QnPlOnvifResourcePtr(new QnFlexWatchResource());
    else if (manufacture.toLower().contains(QLatin1String("vista")))
        resource = QnPlOnvifResourcePtr(new QnVistaResource());
#ifdef ENABLE_AXIS
    else if (manufacture.toLower().contains(QLatin1String("axis")))
        resource = QnPlOnvifResourcePtr(new QnAxisOnvifResource());
#endif
    else
        resource = QnPlOnvifResourcePtr(new QnPlOnvifResource());

    resource->setVendor( manufacture );

    return resource;
}

void OnvifResourceInformationFetcher::pleaseStop()
{
    m_shouldStop = true;
}

#endif //ENABLE_ONVIF
