#include "onvif_resource_information_fetcher.h"
#include "onvif_resource.h"
#include "onvif/soapDeviceBindingProxy.h"
#include "../digitalwatchdog/digital_watchdog_resource.h"
#include "../sony/sony_resource.h"
#include "core/resource_managment/resource_pool.h"
#include "plugins/resources/flex_watch/flexwatch_resource.h"
#include "plugins/resources/axis/axis_onvif_resource.h"

const char* OnvifResourceInformationFetcher::ONVIF_RT = "ONVIF";
const char* ONVIF_ANALOG_RT = "ONVIF_ANALOG";


// Add vendor and camera model to ommit ONVIF search (you have to add in case insensitive here)
static char* ANALOG_CAMERAS[][2] =
{
    {"AXIS", "Q7404"},
	{"vivo_ironman", "VS8801"},
    {"VIVOTEK", "VS8801"}
};

// Add vendor and camera model to ommit ONVIF search (case insensitive)
static char* IGNORE_VENDORS[][2] =
{
    {"*networkcamera*", "dcs-*"}, // DLINK
    {"*", "*spartan-6*"}          // ArecontVision
};

bool OnvifResourceInformationFetcher::isAnalogOnvifResource(const QString& vendor, const QString& model)
{
    for (uint i = 0; i < sizeof(ANALOG_CAMERAS)/sizeof(ANALOG_CAMERAS[0]); ++i)
    {
        if (vendor.compare(QString(lit(ANALOG_CAMERAS[i][0])), Qt::CaseInsensitive) == 0 && 
            model.compare(QString(lit(ANALOG_CAMERAS[i][1])), Qt::CaseInsensitive) == 0)
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

    typePtr = QnResourceTypePtr (qnResTypePool->getResourceTypeByName(lit(ONVIF_ANALOG_RT)));
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

bool OnvifResourceInformationFetcher::ignoreCamera(const EndpointAdditionalInfo& info) const
{
    for (uint i = 0; i < sizeof(IGNORE_VENDORS)/sizeof(IGNORE_VENDORS[0]); ++i)
    {
        QRegExp rxVendor(QLatin1String(IGNORE_VENDORS[i][0]), Qt::CaseInsensitive, QRegExp::Wildcard);
        QRegExp rxName(QLatin1String(IGNORE_VENDORS[i][1]), Qt::CaseInsensitive, QRegExp::Wildcard);

        if (rxVendor.exactMatch(info.manufacturer) && rxName.exactMatch(info.name))
            return true;
    }
    return false;
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

    if (ignoreCamera(info))
        return;

    if (camersNamesData.isManufacturerSupported(info.manufacturer) && camersNamesData.isSupported(info.name)) {
        //qDebug() << "OnvifResourceInformationFetcher::findResources: skipping camera " << info.name;
        return;
    }

    QString manufacturer = info.manufacturer;
    QString model = info.name;
    QString firmware;
    QHostAddress sender(QUrl(endpoint).host());
    //TODO:UTF unuse std::string
    DeviceSoapWrapper soapWrapper(endpoint.toStdString(), std::string(), std::string(), 0);

    QnVirtualCameraResourcePtr existResource = qnResPool->getNetResourceByPhysicalId(info.uniqId).dynamicCast<QnVirtualCameraResource>();
    if (existResource)
        soapWrapper.setLoginPassword(existResource->getAuth().user().toStdString(), existResource->getAuth().password().toStdString());
    else if (!info.defaultLogin.isEmpty())
        soapWrapper.setLoginPassword(info.defaultLogin.toStdString(), info.defaultPassword.toStdString());
    else
        soapWrapper.fetchLoginPassword(info.manufacturer);
    //qDebug() << "OnvifResourceInformationFetcher::findResources: Initial login = " << soapWrapper.getLogin() << ", password = " << soapWrapper.getPassword();

    //some cameras returns by default not specific names; for example vivoteck returns "networkcamera" -> just in case we request params one more time.

    //Trying to get name and manufacturer
    if (existResource)
    {
        if (model.isEmpty())
            model = existResource->getModel();
        QnResourceTypePtr resType = qnResTypePool->getResourceType(existResource->getTypeId());
        if (manufacturer.isEmpty())
            manufacturer = resType->getName();
    }
    else if (model.isEmpty() || manufacturer.isEmpty())
    {
        DeviceInfoReq request;
        DeviceInfoResp response;
        int soapRes = soapWrapper.getDeviceInformation(request, response);
        if (soapRes != SOAP_OK) {
            qDebug() << "OnvifResourceInformationFetcher::findResources: SOAP to endpoint '" << endpoint
                     << "' failed. Camera name will be set to 'Unknown'. GSoap error code: " << soapRes
                     << ". " << soapWrapper.getLastError();
        } 
        else {
            if (!response.Manufacturer.empty())
                manufacturer = QString::fromStdString(response.Manufacturer);

            if (model.isEmpty())
                model = QString::fromStdString(response.Model);

            if (!response.FirmwareVersion.empty())
                firmware = QString::fromStdString(response.FirmwareVersion);

            if (camersNamesData.isManufacturerSupported(manufacturer) && camersNamesData.isSupported(QString(model).replace(manufacturer, QString()))) {
                qDebug() << "OnvifResourceInformationFetcher::findResources: (later step) skipping camera " << model;
                return;
            }
        }
    }

    if (model.isEmpty()) {
        qWarning() << "OnvifResourceInformationFetcher::findResources: can't fetch name of ONVIF device: endpoint: " << endpoint << ", UniqueId: " << info.uniqId;
        model = info.uniqId;
    }


    QnPlOnvifResourcePtr res = createResource(manufacturer, firmware, QHostAddress(sender), QHostAddress(info.discoveryIp),
                                              model, mac, info.uniqId, QString::fromStdString(soapWrapper.getLogin()), QString::fromStdString(soapWrapper.getPassword()), endpoint);
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
                model, mac, info.uniqId, QString::fromStdString(soapWrapper.getLogin()), QString::fromStdString(soapWrapper.getPassword()), endpoint);
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

QnId OnvifResourceInformationFetcher::getOnvifResourceType(const QString& manufacturer, const QString&  model) const
{
    QnId rt = qnResTypePool->getResourceTypeId(QLatin1String("OnvifDevice"), manufacturer, false); // try to find child resource type, use real manufacturer name as camera model in onvif XML
    if (rt.isValid())
        return rt;
    else if (isAnalogOnvifResource(manufacturer, model) && onvifAnalogTypeId.isValid())
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

    /*
    QnId rt = qnResTypePool->getResourceTypeId(QLatin1String("OnvifDevice"), manufacturer, false); // try to find child resource type, use real manufacturer name as camera model in onvif XML
    if (rt.isValid())
        resource->setTypeId(rt);
    else if (isAnalogOnvifResource(manufacturer, model) && onvifAnalogTypeId.isValid())
        resource->setTypeId(onvifAnalogTypeId);
    else 
        resource->setTypeId(onvifTypeId); // no child resourceType found. Use root ONVIF resource type
    */

    resource->setHostAddress(QHostAddress(sender).toString(), QnDomainMemory);
    resource->setDiscoveryAddr(discoveryIp);
    //resource->setName(manufacturer + QLatin1String(" - ") + name);
    resource->setModel(model);
    if (isModelContainVendor(manufacturer, model))
        resource->setName(model); 
    else
        resource->setName(manufacturer + model); 
    resource->setMAC(mac);
    resource->setFirmware(firmware);

    if (!mac.size())
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
    //TODO:UTF unuse std::string
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
    else if (manufacture.toLower().contains(QLatin1String("axis")))
        resource = QnPlOnvifResourcePtr(new QnAxisOnvifResource());
    else
        resource = QnPlOnvifResourcePtr(new QnPlOnvifResource());

    return resource;
}

void OnvifResourceInformationFetcher::pleaseStop()
{
    m_shouldStop = true;
}
