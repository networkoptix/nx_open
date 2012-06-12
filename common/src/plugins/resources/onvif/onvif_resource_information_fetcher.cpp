#ifdef WIN32
#include "openssl/evp.h"
#else
#include "evp.h"
#endif

#include "onvif_resource_information_fetcher.h"
//#include "core/resource/camera_resource.h"
#include "onvif_resource.h"
#include "onvif/Onvif.nsmap"
#include "onvif/soapDeviceBindingProxy.h"
#include "onvif/wsseapi.h"

const char* OnvifResourceInformationFetcher::ONVIF_RT = "ONVIF";
std::string& OnvifResourceInformationFetcher::STD_ONVIF_USER = *(new std::string("admin"));
std::string& OnvifResourceInformationFetcher::STD_ONVIF_PASSWORD = *(new std::string("admin"));


OnvifResourceInformationFetcher::OnvifResourceInformationFetcher()
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

    QHostAddress sender(hostAddressFromEndpoint(endpoint));
    const char* login = 0;
    const char* passwd = 0;
    QString manufacturer("");//ToDo: find manufacturer
    PasswordList passwords = passwordsData.getPasswordsByManufacturer(manufacturer);
    PasswordList::ConstIterator passwdIter = passwords.begin();

    int soapRes = SOAP_OK;
    DeviceBindingProxy soapProxy;
    soapProxy.soap->send_timeout = -200000;
    soapProxy.soap->recv_timeout = -200000;
    soap_register_plugin(soapProxy.soap, soap_wsse);

    //Trying to pick a password
    do {
        if (login) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login, passwd);

        qDebug() << "Trying login = " << login << ", password = " << passwd;

        _onvifDevice__GetCapabilities request1;
        request1.Category.push_back(onvifXsd__CapabilityCategory__All);
        _onvifDevice__GetCapabilitiesResponse response1;
        soapRes = soapProxy.GetCapabilities(endpoint.toStdString().c_str(), NULL, &request1, &response1);

        if (soapRes == SOAP_OK || !passwordsData.isNotAuthenticated(soapProxy.soap_fault())) {
            qDebug() << "Finished picking password";
            soap_end(soapProxy.soap);
            break;
        }

        soap_end(soapProxy.soap);

        if (passwdIter == passwords.end()) {
            qDebug() << "Trying to create a password";

            //If we had no luck in picking a password, let's try to create a user
            onvifXsd__User newUser;
            newUser.Username = STD_ONVIF_USER;
            newUser.Password = &STD_ONVIF_PASSWORD;

            for (int i = 0; i <= 2; ++i) {
                _onvifDevice__CreateUsers request2;
                _onvifDevice__CreateUsersResponse response2;

                switch (i) {
                    case 0: newUser.UserLevel = onvifXsd__UserLevel__Administrator; qDebug() << "Trying to create Admin"; break;
                    case 1: newUser.UserLevel = onvifXsd__UserLevel__Operator; qDebug() << "Trying to create Operator"; break;
                    case 2: newUser.UserLevel = onvifXsd__UserLevel__User; qDebug() << "Trying to create Regular User"; break;
                    default: qWarning() << "OnvifResourceInformationFetcher::findResources: unknown user index."; break;
                }
                request2.User.push_back(&newUser);

                soapRes = soapProxy.CreateUsers(endpoint.toStdString().c_str(), NULL, &request2, &response2);
                if (soapRes == SOAP_OK) {
                    qDebug() << "User created!";
                    login = STD_ONVIF_USER.c_str();
                    passwd = STD_ONVIF_PASSWORD.c_str();
                    soap_end(soapProxy.soap);
                    break;
                }

                qDebug() << "User is NOT created";
                login = 0;
                passwd = 0;
                soap_end(soapProxy.soap);
            }

            break;
        }

        login = passwdIter->first;
        passwd = passwdIter->second;
        ++passwdIter;

    } while (true);

    qDebug() << "OnvifResourceInformationFetcher::findResources: Initial login = " << login << ", password = " << passwd;

    //Trying to get MAC address
    _onvifDevice__GetNetworkInterfaces request1;
    _onvifDevice__GetNetworkInterfacesResponse response1;
    if (login) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login, passwd);
    soapRes = soapProxy.GetNetworkInterfaces(endpoint.toStdString().c_str(), NULL, &request1, &response1);
    if (soapRes != SOAP_OK && cl_log.logLevel() >= cl_logDEBUG1) {
        qDebug() << "OnvifResourceInformationFetcher::findResources: SOAP to endpoint '" << endpoint
                 << "' failed. Can't fetch MAC, will try to get analog. GSoap error code: " << soapRes
                 << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());
    }
    soap_end(soapProxy.soap);
    QString mac(fetchMacAddress(response1, sender.toString()));
    if (isMacAlreadyExists(mac, result)) {
        return;
    }

    //Trying to get name and possibly MAC
    _onvifDevice__GetDeviceInformation request2;
    _onvifDevice__GetDeviceInformationResponse response2;
    if (login) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login, passwd);
    soapRes = soapProxy.GetDeviceInformation(endpoint.toStdString().c_str(), NULL, &request2, &response2);
    if (soapRes != SOAP_OK) {
        qDebug() << "OnvifResourceInformationFetcher::findResources: SOAP to endpoint '" << endpoint
                 << "' failed. Camera name will be set to 'Unknown'. GSoap error code: " << soapRes
                 << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());
    }

    if (mac.isEmpty()) {
        mac = fetchSerialConvertToMac(response2);
        if (mac.isEmpty()) {
            if (passwordsData.isNotAuthenticated(soapProxy.soap_fault())) {
                qCritical() << "OnvifResourceInformationFetcher::findResources: Can't get ONVIF device MAC address, because login/password required. Endpoint URL: " << endpoint;
            }

            soap_end(soapProxy.soap);
            return;
        }

        if (isMacAlreadyExists(mac, result)) {
            soap_end(soapProxy.soap);
            return;
        }
    }
    soap_end(soapProxy.soap);

    QString name(fetchName(response2));
    if (name.isEmpty()) {
        qWarning() << "OnvifResourceInformationFetcher::findResources: can't fetch name of ONVIF device: endpoint: " << endpoint
                   << ", MAC: " << mac;
        name = "Unknown - " + mac;
    }

    if (manufacturer.isEmpty()) { //ToDo" fetch manufacturer
    }
    qDebug() << "OnvifResourceInformationFetcher::findResources: manufacturer: " << manufacturer;

    //Trying to get onvif URLs
    QString mediaUrl;
    QString deviceUrl;
    {
        _onvifDevice__GetCapabilities request;
        _onvifDevice__GetCapabilitiesResponse response;
        if (login) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login, passwd);

        soapRes = soapProxy.GetCapabilities(endpoint.toStdString().c_str(), NULL, &request, &response);
        if (soapRes != SOAP_OK && cl_log.logLevel() >= cl_logDEBUG1) {
            qDebug() << "OnvifResourceInformationFetcher::findResources: can't fetch media and device URLs. Reason: SOAP to endpoint "
                     << endpoint << " failed. GSoap error code: " << soapRes << SoapErrorHelper::fetchDescription(soapProxy.soap_fault());
        }
        soap_end(soapProxy.soap);

        if (response.Capabilities) {
            mediaUrl = response.Capabilities->Media? response.Capabilities->Media->XAddr.c_str(): mediaUrl;
            deviceUrl = response.Capabilities->Device? response.Capabilities->Device->XAddr.c_str(): deviceUrl;
        }
    }

    qDebug() << "OnvifResourceInformationFetcher::createResource: Found new camera: endpoint: " << endpoint
             << ", MAC: " << mac << ", name: " << name;

    createResource(manufacturer, QHostAddress(sender), QHostAddress(info.discoveryIp),
        name, mac, login, passwd, mediaUrl, deviceUrl, result);
}

void OnvifResourceInformationFetcher::createResource(const QString& manufacturer, const QHostAddress& sender, const QHostAddress& discoveryIp,
    const QString& name, const QString& mac, const char* login, const char* passwd, const QString& mediaUrl, const QString& deviceUrl, QnResourceList& result) const
{
    QnNetworkResourcePtr resource(0);

    if (resource.isNull()) {
        resource = QnNetworkResourcePtr(new QnPlOnvifResource());
        resource->setTypeId(onvifTypeId);
    }

    resource->setHostAddress(sender, QnDomainMemory);
    resource->setDiscoveryAddr(discoveryIp);
    resource->setName(name);
    resource->setMAC(mac);

    if (login) {
        qDebug() << "OnvifResourceInformationFetcher::createResource: Setting login = " << login << ", password = " << passwd;
        resource->setAuth(login, passwd);
    }

    if (!mediaUrl.isEmpty()) {
        qDebug() << "OnvifResourceInformationFetcher::createResource: Setting mediaUrl = " << mediaUrl;
        resource->setParam(QnPlOnvifResource::MEDIA_URL_PARAM_NAME, mediaUrl, QnDomainDatabase);
    }

    if (!deviceUrl.isEmpty()) {
        qDebug() << "OnvifResourceInformationFetcher::createResource: Setting deviceUrl = " << deviceUrl;
        resource->setParam(QnPlOnvifResource::DEVICE_URL_PARAM_NAME, deviceUrl, QnDomainDatabase);
    }

    result.push_back(resource);
}

const bool OnvifResourceInformationFetcher::isMacAlreadyExists(const QString& mac, const QnResourceList& resList) const
{
    foreach(QnResourcePtr res, resList) {
        QnNetworkResourcePtr netRes = res.dynamicCast<QnNetworkResource>();

        if (netRes->getMAC().toString() == mac) {
            return true;
        }
    }

    return false;
}

const QString OnvifResourceInformationFetcher::fetchMacAddress(const _onvifDevice__GetNetworkInterfacesResponse& response,
        const QString& senderIpAddress) const
{
    QString someMacAddress;
    std::vector<class onvifXsd__NetworkInterface*> ifaces = response.NetworkInterfaces;
    std::vector<class onvifXsd__NetworkInterface*>::const_iterator ifacePtrIter = ifaces.begin();

    while (ifacePtrIter != ifaces.end()) {
        onvifXsd__NetworkInterface* ifacePtr = *ifacePtrIter;

        if (ifacePtr->Enabled && ifacePtr->IPv4->Enabled) {
            onvifXsd__IPv4Configuration* conf = ifacePtr->IPv4->Config;

            if (conf->DHCP) {
                if (senderIpAddress == conf->FromDHCP->Address.c_str()) {
                    return QString(ifacePtr->Info->HwAddress.c_str()).toUpper().replace(":", "-");
                }
                if (someMacAddress.isEmpty()) {
                    someMacAddress = QString(ifacePtr->Info->HwAddress.c_str());
                }
            }

            std::vector<class onvifXsd__PrefixedIPv4Address*> addresses = ifacePtr->IPv4->Config->Manual;
            std::vector<class onvifXsd__PrefixedIPv4Address*>::const_iterator addrPtrIter = addresses.begin();

            while (addrPtrIter != addresses.end()) {
                onvifXsd__PrefixedIPv4Address* addrPtr = *addrPtrIter;

                if (senderIpAddress == addrPtr->Address.c_str()) {
                    return QString(ifacePtr->Info->HwAddress.c_str()).toUpper().replace(":", "-");
                }
                if (someMacAddress.isEmpty()) {
                    someMacAddress = QString(ifacePtr->Info->HwAddress.c_str());
                }

                ++addrPtrIter;
            }
        }

        ++ifacePtrIter;
    }

    qDebug() << "OnvifResourceInformationFetcher::fetchMacAddress: appropriate MAC address was not found for device with IP: "
             << senderIpAddress.toStdString().c_str();
    return someMacAddress.toUpper().replace(":", "-");
}

const QString OnvifResourceInformationFetcher::fetchName(const _onvifDevice__GetDeviceInformationResponse& response) const
{
    return QString((response.Manufacturer + " - " + response.Model).c_str());
}

const QString OnvifResourceInformationFetcher::fetchManufacturer(const _onvifDevice__GetDeviceInformationResponse& response) const
{
    return QString(response.Manufacturer.c_str());
}

const QString OnvifResourceInformationFetcher::fetchSerialConvertToMac(const _onvifDevice__GetDeviceInformationResponse& response) const
{
    QString serialAndHwdId(response.HardwareId.c_str());
    serialAndHwdId += response.SerialNumber.c_str();
    serialAndHwdId.replace(QRegExp("[^\\w\\d]+"), "");

    //Too small serial number (may be, its better to check < 12)
    int size = serialAndHwdId.size();
    if (size < 6) return QString();

    QString result("FF-FF-FF-FF-FF-FF");
    for (int i = 1; i < (size < 12? size: 12); i += 2) {
        result[i - 1 + i / 2] = serialAndHwdId[i - 1];
        result[i + i / 2] = serialAndHwdId[i];
    }
    if (size < 12 && size % 2) {
        --size;
        result[size + size / 2] = serialAndHwdId[size];
    }

    return result;
}

//const QString OnvifResourceInformationFetcher::generateRandomPassword() const
//{
//    QTime curTime(QTime::currentTime());
//    qsrand((curTime.hour() + 1) * (curTime.minute() + 1));
//
//    QString tmp;
//    QString result;
//    result += tmp.setNum(qrand());
//    result += tmp.setNum(curTime.second());
//    result += tmp.setNum(qrand());
//    result += tmp.setNum(curTime.msec());
//
//    qDebug() << "Random password is " << result;
//    return result;
//}

QHostAddress OnvifResourceInformationFetcher::hostAddressFromEndpoint(const QString& endpoint) const
{
    char marker[] = "://";
    int pos1 = endpoint.indexOf(marker);
    if (pos1 == -1) {
        qWarning() << "OnvifResourceInformationFetcher::hostAddressFromEndpoint: invalid URL: " << endpoint;
        return QHostAddress();
    }

    pos1 += sizeof(marker) - 1;
    int pos2 = endpoint.indexOf(":", pos1);
    int pos3 = endpoint.indexOf("/", pos1);
    //Nearest match:
    pos2 = pos2 < pos3 && pos2 != -1? pos2: (pos3 != -1? pos3: pos2);
    //Size:
    pos2 = pos2 == -1? -1: pos2 - pos1;

    QString tmp = endpoint.mid(pos1, pos2);
    qDebug() << "OnvifResourceInformationFetcher::hostAddressFromEndpoint: IP: " << tmp;
    return QHostAddress(tmp);
}
