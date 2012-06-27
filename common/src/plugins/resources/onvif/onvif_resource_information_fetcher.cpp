//#ifdef WIN32
//#include "openssl/evp.h"
//#else
//#include "evp.h"
//#endif

#include "onvif_resource_information_fetcher.h"
//#include "core/resource/camera_resource.h"
#include "onvif_resource.h"
//#include "onvif/Onvif.nsmap"
#include "onvif/soapDeviceBindingProxy.h"
//#include "onvif/wsseapi.h"
#include "../digitalwatchdog/digital_watchdog_resource.h"

const char* OnvifResourceInformationFetcher::ONVIF_RT = "ONVIF";
std::string& OnvifResourceInformationFetcher::STD_ONVIF_USER = *(new std::string("admin"));
std::string& OnvifResourceInformationFetcher::STD_ONVIF_PASSWORD = *(new std::string("admin"));


OnvifResourceInformationFetcher::OnvifResourceInformationFetcher():
    /*passwordsData(PasswordHelper::instance()),*/
    camersNamesData(NameHelper::instance())
{
	QnResourceTypePtr typePtr(qnResTypePool->getResourceTypeByName(ONVIF_RT));
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

    while(iter != endpointInfo.end()) {
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

    if (camersNamesData.isSupported(info.name)) {
        qDebug() << "OnvifResourceInformationFetcher::findResources: skipping camera " << info.name;
        return;
    }

    QString manufacturer = info.manufacturer;
    QString name = info.name;
    QHostAddress sender(QUrl(endpoint).host());
    DeviceSoapWrapper soapWrapper(endpoint.toStdString(), std::string(), std::string());

    soapWrapper.fetchLoginPassword(info.manufacturer);
    qDebug() << "OnvifResourceInformationFetcher::findResources: Initial login = " << soapWrapper.getLogin() << ", password = " << soapWrapper.getPassword();

    //Trying to get MAC
    {
        NetIfacesReq request;
        NetIfacesResp response;
        soapWrapper.getNetworkInterfaces(request, response);
        QString tmp = QnPlOnvifResource::fetchMacAddress(response, sender.toString());
        qDebug() << "Fetched MAC: " << tmp << " for endpoint: " << endpoint;
        if (!tmp.isEmpty() && mac != tmp) {
            mac = tmp;
            if (isMacAlreadyExists(info.uniqId, result))
                return;
        }
    }

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

                if (camersNamesData.isSupported(QString(name).replace(manufacturer, ""))) {
                    qDebug() << "OnvifResourceInformationFetcher::findResources: (later step) skipping camera " << name;
                    return;
                }
            }
        }
    }

    if (name.isEmpty()) {
        qWarning() << "OnvifResourceInformationFetcher::findResources: can't fetch name of ONVIF device: endpoint: " << endpoint
            << ", UniqueId: " << info.uniqId;
        name = "Unknown - " + info.uniqId;
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
            mediaUrl = response.Capabilities->Media? response.Capabilities->Media->XAddr.c_str(): mediaUrl;
            deviceUrl = response.Capabilities->Device? response.Capabilities->Device->XAddr.c_str(): deviceUrl;
        }

        if (deviceUrl.isEmpty()) {
            deviceUrl = endpoint;
        }
        if (mediaUrl.isEmpty()) {
            mediaUrl = endpoint;
        }
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

    QnId rt = qnResTypePool->getResourceTypeId("OnvifDevice", manufacturer); // try to find child resource type, use real manufacturer name as camera model in onvif XML
    if (rt.isValid())
        resource->setTypeId(rt);
    else
        resource->setTypeId(onvifTypeId); // no child resourceType found. Use root ONVIF resource type

    resource->setHostAddress(sender, QnDomainMemory);
    resource->setDiscoveryAddr(discoveryIp);
    resource->setName(manufacturer + " - " + name);
    resource->setMAC(mac);

    if (!mac.size())
        resource->setPhysicalId(uniqId);

    resource->setMediaUrl(mediaUrl);
    resource->setDeviceOnvifUrl(deviceUrl);

    if (login) {
        qDebug() << "OnvifResourceInformationFetcher::createResource: Setting login = " << login << ", password = " << passwd;
        resource->setAuth(login, passwd);
    }

    result.push_back(resource);
}

const bool OnvifResourceInformationFetcher::isMacAlreadyExists(const QString& mac, const QnResourceList& resList) const
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

const QString OnvifResourceInformationFetcher::fetchName(const DeviceInfoResp& response) const
{
    return QString(response.Model.c_str());
}

const QString OnvifResourceInformationFetcher::fetchManufacturer(const DeviceInfoResp& response) const
{
    return QString(response.Manufacturer.c_str());
}

const QString OnvifResourceInformationFetcher::fetchSerial(const DeviceInfoResp& response) const
{
    return (response.HardwareId.empty()? QString(): QString((response.HardwareId + "::").c_str())) +
        (response.SerialNumber.empty()? QString(): QString(response.SerialNumber.c_str()));
}

QnPlOnvifResourcePtr OnvifResourceInformationFetcher::createOnvifResourceByManufacture(const QString& manufacture)
{
    QnPlOnvifResourcePtr resource;
    if (manufacture.toLower().contains("digital watchdog"))
        resource = QnPlOnvifResourcePtr(new QnPlWatchDogResource());
    else
        resource = QnPlOnvifResourcePtr(new QnPlOnvifResource());

    return resource;
}