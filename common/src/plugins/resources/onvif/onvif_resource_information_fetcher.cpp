#include "onvif_resource_information_fetcher.h"
#include "onvif_resource.h"
#include "onvif/soapDeviceBindingProxy.h"
#include "../digitalwatchdog/digital_watchdog_resource.h"
#include "../sony/sony_resource.h"
#include "core/resource_managment/resource_pool.h"
#include "plugins/resources/flex_watch/flexwatch_resource.h"

const char* OnvifResourceInformationFetcher::ONVIF_RT = "ONVIF";


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

    if (camersNamesData.isManufacturerSupported(info.manufacturer) && camersNamesData.isSupported(info.name)) {
        //qDebug() << "OnvifResourceInformationFetcher::findResources: skipping camera " << info.name;
        return;
    }

    QString manufacturer = info.manufacturer;
    QString name = info.name;
    QHostAddress sender(QUrl(endpoint).host());
    //TODO:UTF unuse std::string
    DeviceSoapWrapper soapWrapper(endpoint.toStdString(), std::string(), std::string(), 0);

    QnVirtualCameraResourcePtr existResource = qnResPool->getNetResourceByPhysicalId(info.uniqId).dynamicCast<QnVirtualCameraResource>();
    if (existResource)
        soapWrapper.setLoginPassword(existResource->getAuth().user().toStdString(), existResource->getAuth().password().toStdString());
    else
        soapWrapper.fetchLoginPassword(info.manufacturer);
    //qDebug() << "OnvifResourceInformationFetcher::findResources: Initial login = " << soapWrapper.getLogin() << ", password = " << soapWrapper.getPassword();

    //some cameras returns by default not specific names; for example vivoteck returns "networkcamera" -> just in case we request params one more time.

    //Trying to get name and manufacturer
    if (existResource)
    {
        name = existResource->getModel();
        QnResourceTypePtr resType = qnResTypePool->getResourceType(existResource->getTypeId());
        manufacturer = resType->getName();
    }
    else
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

            if (!response.Model.empty())
                name = QString::fromStdString(response.Model);

            if (camersNamesData.isManufacturerSupported(manufacturer) && camersNamesData.isSupported(QString(name).replace(manufacturer, QString()))) {
                qDebug() << "OnvifResourceInformationFetcher::findResources: (later step) skipping camera " << name;
                return;
            }
        }
    }

    if (name.isEmpty()) {
        qWarning() << "OnvifResourceInformationFetcher::findResources: can't fetch name of ONVIF device: endpoint: " << endpoint
            << ", UniqueId: " << info.uniqId;
        name = tr("Unknown - %1").arg(info.uniqId);
    }


    QnPlOnvifResourcePtr res = createResource(manufacturer, QHostAddress(sender), QHostAddress(info.discoveryIp),
                                              name, mac, info.uniqId, QString::fromStdString(soapWrapper.getLogin()), QString::fromStdString(soapWrapper.getPassword()), endpoint);
    if (res)
        result << res;
    else
        return;

    // checking for multichannel encoders
    QnPlOnvifResourcePtr onvifRes = existResource.dynamicCast<QnPlOnvifResource>();
    if (onvifRes) {
        for (int i = 1; i < onvifRes->getMaxChannels(); ++i) 
        {
            res = createResource(manufacturer, QHostAddress(sender), QHostAddress(info.discoveryIp),
                name, mac, info.uniqId, QString::fromStdString(soapWrapper.getLogin()), QString::fromStdString(soapWrapper.getPassword()), endpoint);
            if (res) {
                QString suffix = QString(QLatin1String("?channel=%1")).arg(i+1);
                res->setUrl(endpoint + suffix);
                res->setPhysicalId(info.uniqId + suffix.replace(QLatin1String("?"), QLatin1String("_")));
                res->setName(res->getName() + QString(QLatin1String("-channel %1")).arg(i+1));
                result << res;
            }
        }
    }
}

QnPlOnvifResourcePtr OnvifResourceInformationFetcher::createResource(const QString& manufacturer, const QHostAddress& sender, const QHostAddress& discoveryIp, const QString& name, 
    const QString& mac, const QString& uniqId, const QString& login, const QString& passwd, const QString& deviceUrl) const
{
    if (uniqId.isEmpty())
        return QnPlOnvifResourcePtr();

    QnPlOnvifResourcePtr resource = createOnvifResourceByManufacture(manufacturer);
    if (!resource)
        return resource;

    QnId rt = qnResTypePool->getResourceTypeId(QLatin1String("OnvifDevice"), manufacturer, false); // try to find child resource type, use real manufacturer name as camera model in onvif XML
    if (rt.isValid())
        resource->setTypeId(rt);
    else
        resource->setTypeId(onvifTypeId); // no child resourceType found. Use root ONVIF resource type

    resource->setHostAddress(QHostAddress(sender).toString(), QnDomainMemory);
    resource->setDiscoveryAddr(discoveryIp);
    //resource->setName(manufacturer + QLatin1String(" - ") + name);
    resource->setModel(name);
    resource->setName(name); 
    resource->setMAC(mac);

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
    else
        resource = QnPlOnvifResourcePtr(new QnPlOnvifResource());

    return resource;
}

void OnvifResourceInformationFetcher::pleaseStop()
{
    m_shouldStop = true;
}
