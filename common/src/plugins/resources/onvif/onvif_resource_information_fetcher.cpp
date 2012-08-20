#include "onvif_resource_information_fetcher.h"
#include "onvif_resource.h"
#include "onvif/soapDeviceBindingProxy.h"
#include "../digitalwatchdog/digital_watchdog_resource.h"
#include "../sony/sony_resource.h"

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

    /*if (camersNamesData.isManufacturerSupported(info.manufacturer) && camersNamesData.isSupported(info.name)) {
        qDebug() << "OnvifResourceInformationFetcher::findResources: skipping camera " << info.name;
        return;
    }*/

    QString manufacturer = info.manufacturer;
    QString name = info.name;
    QHostAddress sender(QUrl(endpoint).host());
    //TODO:UTF unuse std::string
    DeviceSoapWrapper soapWrapper(endpoint.toStdString(), std::string(), std::string());

    soapWrapper.fetchLoginPassword(info.manufacturer);
    qDebug() << "OnvifResourceInformationFetcher::findResources: Initial login = " << soapWrapper.getLogin() << ", password = " << soapWrapper.getPassword();

    //some cameras returns by default not specific names; for example vivoteck returns "networkcamera" -> just in case we request params one more time.

    //Trying to get name and manufacturer
    {
        DeviceInfoReq request;
        DeviceInfoResp response;
        int soapRes = soapWrapper.getDeviceInformation(request, response);
        if (soapRes != SOAP_OK) {
            qDebug() << "OnvifResourceInformationFetcher::findResources: SOAP to endpoint '" << endpoint
                     << "' failed. Camera name will be set to 'Unknown'. GSoap error code: " << soapRes
                     << ". " << soapWrapper.getLastError();
        } else {

            QString tmpManufacturer(fetchManufacturer(response));
            if (!tmpManufacturer.isEmpty() && manufacturer != tmpManufacturer) {
                manufacturer = tmpManufacturer;
            }

            QString tmpName(fetchName(response));
            if (!tmpName.isEmpty() && name != tmpName) {
                name = tmpName;

                /*if (camersNamesData.isManufacturerSupported(manufacturer) && camersNamesData.isSupported(QString(name).replace(manufacturer, QString()))) {
                    qDebug() << "OnvifResourceInformationFetcher::findResources: (later step) skipping camera " << name;
                    return;
                }*/
            }
        }
    }

    if (name.isEmpty()) {
        qWarning() << "OnvifResourceInformationFetcher::findResources: can't fetch name of ONVIF device: endpoint: " << endpoint
            << ", UniqueId: " << info.uniqId;
        name = tr("Unknown - %1").arg(info.uniqId);
    }

    //Trying to get onvif URLs
    QString mediaUrl;
    QString deviceUrl;
    {
        CapabilitiesReq request;
        CapabilitiesResp response;

        int soapRes = soapWrapper.getCapabilities(request, response);
        if (soapRes != SOAP_OK) {
            qDebug() << "OnvifResourceInformationFetcher::findResources: can't fetch media and device URLs. Reason: SOAP to endpoint "
                     << endpoint << " failed. GSoap error code: " << soapRes << ". " << soapWrapper.getLastError();

        } else if (response.Capabilities) {
            mediaUrl = response.Capabilities->Media? QString::fromStdString(response.Capabilities->Media->XAddr): mediaUrl;
            deviceUrl = response.Capabilities->Device? QString::fromStdString(response.Capabilities->Device->XAddr): deviceUrl;
        }

        if (deviceUrl.isEmpty()) {
            deviceUrl = endpoint;
        }
        //If media url is empty it will be filled in resource init method
    }

    qDebug() << "OnvifResourceInformationFetcher::createResource: Found new camera: endpoint: " << endpoint
             << ", UniqueId: " << info.uniqId << ", manufacturer: " << manufacturer << ", name: " << name;

    createResource(manufacturer, QHostAddress(sender), QHostAddress(info.discoveryIp),
        name, mac, info.uniqId, soapWrapper.getLogin(), soapWrapper.getPassword(), mediaUrl, deviceUrl, result);
}

void OnvifResourceInformationFetcher::createResource(const QString& manufacturer, const QHostAddress& sender, const QHostAddress& discoveryIp, const QString& name, 
    const QString& mac, const QString& uniqId, const char* login, const char* passwd, const QString& mediaUrl, const QString& deviceUrl, QnResourceList& result) const
{
    if (uniqId.isEmpty()) {
        return;
    }

    QnPlOnvifResourcePtr resource = createOnvifResourceByManufacture(manufacturer);
    if (!resource)
        return;

    QnId rt = qnResTypePool->getResourceTypeId(QLatin1String("OnvifDevice"), manufacturer); // try to find child resource type, use real manufacturer name as camera model in onvif XML
    if (rt.isValid())
        resource->setTypeId(rt);
    else
        resource->setTypeId(onvifTypeId); // no child resourceType found. Use root ONVIF resource type

    resource->setHostAddress(sender, QnDomainMemory);
    resource->setDiscoveryAddr(discoveryIp);
    resource->setName(manufacturer + QLatin1String(" - ") + name);
    resource->setMAC(mac);

    if (!mac.size())
        resource->setPhysicalId(uniqId);

    resource->setMediaUrl(mediaUrl);
    resource->setDeviceOnvifUrl(deviceUrl);

    if (login) {
        qDebug() << "OnvifResourceInformationFetcher::createResource: Setting login = " << login << ", password = " << passwd;
        resource->setAuth(QLatin1String(login), QLatin1String(passwd));
    }

    result.push_back(resource);
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

QString OnvifResourceInformationFetcher::fetchName(const DeviceInfoResp& response) const
{
    return QString::fromStdString(response.Model);
}

QString OnvifResourceInformationFetcher::fetchManufacturer(const DeviceInfoResp& response) const
{
    return QString::fromStdString(response.Manufacturer);
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
    else
        resource = QnPlOnvifResourcePtr(new QnPlOnvifResource());

    return resource;
}

void OnvifResourceInformationFetcher::pleaseStop()
{
    m_shouldStop = true;
}
