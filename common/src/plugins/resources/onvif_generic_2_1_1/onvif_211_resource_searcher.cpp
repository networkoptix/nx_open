#ifdef WIN32
#include "openssl/evp.h"
#else
#include "evp.h"
#endif

#include "onvif_211_resource_searcher.h"
#include "core/resource/camera_resource.h"
#include "onvif_211_resource.h"
#include "onvif/Onvif.nsmap"
#include "onvif/soapDeviceBindingProxy.h"
#include "onvif/wsseapi.h"

const char* OnvifGeneric211ResourceSearcher::ONVIF_RT = "ONVIF";


OnvifGeneric211ResourceSearcher::OnvifGeneric211ResourceSearcher()
{
	QnResourceTypePtr typePtr(qnResTypePool->getResourceTypeByName(ONVIF_RT));
	if (!typePtr.isNull()) {
		onvifTypeId = typePtr->getId();
	} else {
		qCritical() << "Can't find " << ONVIF_RT << " resource type in resource type pool";
	}
}

OnvifGeneric211ResourceSearcher& OnvifGeneric211ResourceSearcher::instance()
{
    static OnvifGeneric211ResourceSearcher inst;
    return inst;
}

QnResourcePtr OnvifGeneric211ResourceSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
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
        qDebug() << "Manufature " << resourceType->getManufacture() << " != " << manufacture();
        return result;
    }

    result = QnVirtualCameraResourcePtr( new QnPlOnvifGeneric211Resource( ) );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create ONVIF camera resource. TypeID: " << resourceTypeId.toString() << ", Parameters: " << parameters;
    result->deserialize(parameters);

    return result;

}

QString OnvifGeneric211ResourceSearcher::manufacture() const
{
    return QnPlOnvifGeneric211Resource::MANUFACTURE;
}


QnResourcePtr OnvifGeneric211ResourceSearcher::checkHostAddr(QHostAddress addr)
{
    return QnResourcePtr(0);
}

QnNetworkResourcePtr OnvifGeneric211ResourceSearcher::processPacket(QnResourceList& result, QByteArray& mdnsPacketData, const QHostAddress& sender)
{
    QString endpoint(QnPlOnvifGeneric211Resource::createOnvifEndpointUrl(sender.toString()));
    if (endpoint.isEmpty()) {
        qDebug() << "OnvifGeneric211ResourceSearcher::processPacket: response packet was received, but appropriate URL was not found.";
        return QnNetworkResourcePtr(0);
    }

    const char* login = 0;
    const char* passwd = 0;
    std::string onvifUser = "netoptix_onvif";
    std::string onvifPasswd;
    PasswordList passwords = passHelper.getPasswordsByManufacturer(mdnsPacketData);
    PasswordList::ConstIterator passwdIter = passwords.begin();

    int soapRes = SOAP_OK;
    DeviceBindingProxy soapProxy;
    soap_register_plugin(soapProxy.soap, soap_wsse);

    //Trying to pick a password
    do {
        if (login) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login, passwd);

        qDebug() << "Trying login = " << login << ", password = " << passwd;

        _onvifDevice__GetCapabilities request1;
        request1.Category.push_back(onvifXsd__CapabilityCategory__All);
        _onvifDevice__GetCapabilitiesResponse response1;
        soapRes = soapProxy.GetCapabilities(endpoint.toStdString().c_str(), NULL, &request1, &response1);

        if (soapRes == SOAP_OK || !passHelper.isNotAuthenticated(soapProxy.soap_fault())) { qDebug() << "Finished"; break; }
        if (passwdIter == passwords.end()) {
            qDebug() << "Trying to create a password";

            //If we had no luck in picking a password, let's try to create a user
            onvifPasswd = generateRandomPassword().toStdString();
            onvifXsd__User newUser;
            newUser.Username = onvifUser;
            newUser.Password = &onvifPasswd;

            for (int i = 0; i <= 2; ++i) {
                _onvifDevice__CreateUsers request2;
                _onvifDevice__CreateUsersResponse response2;

                switch (i) {
                    case 0: newUser.UserLevel = onvifXsd__UserLevel__Administrator; qDebug() << "Trying to create Admin"; break;
                    case 1: newUser.UserLevel = onvifXsd__UserLevel__Operator; qDebug() << "Trying to create Operator"; break;
                    case 2: newUser.UserLevel = onvifXsd__UserLevel__User; qDebug() << "Trying to create Regular User"; break;
                    default: qWarning() << "OnvifGeneric211ResourceSearcher::processPacket: unknown user index."; break;
                }
                request2.User.push_back(&newUser);

                soapRes = soapProxy.CreateUsers(endpoint.toStdString().c_str(), NULL, &request2, &response2);
                if (soapRes == SOAP_OK) {
                    qDebug() << "Usser created!";
                    login = onvifUser.c_str();
                    passwd = onvifPasswd.c_str();
                    break;
                } else {
                    qDebug() << "Usser is NOT created";
                    login = 0;
                    passwd = 0;
                }
            }

            break;
        }

        login = passwdIter->first;
        passwd = passwdIter->second;
        ++passwdIter;

    } while (true);

    qDebug() << "OnvifGeneric211ResourceSearcher::processPacket: Initial login = " << login << ", password = " << passwd;

    //Trying to get MAC address
    _onvifDevice__GetNetworkInterfaces request1;
    _onvifDevice__GetNetworkInterfacesResponse response1;
    if (login) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login, passwd);
    soapRes = soapProxy.GetNetworkInterfaces(endpoint.toStdString().c_str(), NULL, &request1, &response1);
    if (soapRes != SOAP_OK && cl_log.logLevel() >= cl_logDEBUG1) {
        qDebug() << "OnvifGeneric211ResourceSearcher::processPacket: SOAP to endpoint '" << endpoint
                 << "' failed. Error code: " << soapRes << "Description: "
                 << soapProxy.soap_fault_string() << ". " << soapProxy.soap_fault_detail()
                 << ". Can't fetch MAC, will try to get analog." ;
    }
    QString mac(fetchMacAddress(response1, sender.toString()));
    if (isMacAlreadyExists(mac, result)) {
        return QnNetworkResourcePtr(0);
    }

    //Trying to get name and possibly MAC
    _onvifDevice__GetDeviceInformation request2;
    _onvifDevice__GetDeviceInformationResponse response2;
    if (login) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login, passwd);
    soapRes = soapProxy.GetDeviceInformation(endpoint.toStdString().c_str(), NULL, &request2, &response2);
    if (soapRes != SOAP_OK) {
        qDebug() << "OnvifGeneric211ResourceSearcher::processPacket: SOAP to endpoint '" << endpoint
                 << "' failed. Error code: " << soapRes << "Description: " << soapProxy.soap_fault_string() << ". "
                 << soapProxy.soap_fault_detail() << ". Camera name will be set to 'Unknown'.";
    }

    if (mac.isEmpty()) {
        mac = fetchSerialConvertToMac(response2);
        if (mac.isEmpty()) {
            if (passHelper.isNotAuthenticated(soapProxy.soap_fault())) {
                qCritical() << "OnvifGeneric211ResourceSearcher::processPacket: Can't get ONVIF device MAC address, because login/password required. Endpoint URL: " << endpoint;
            }

            return QnNetworkResourcePtr(0);
        }

        if (isMacAlreadyExists(mac, result)) {
            return QnNetworkResourcePtr(0);
        }
    }

    QString name(fetchName(response2));
    if (name.isEmpty()) {
        qWarning() << "OnvifGeneric211ResourceSearcher::processPacket: can't fetch name of ONVIF device: endpoint: " << endpoint
                   << ", MAC: " << mac;
        name = "Unknown - " + mac;
    }

    //Trying to get onvif URLs
    QString mediaUrl;
    QString deviceUrl;
    {
        _onvifDevice__GetCapabilities request;
        _onvifDevice__GetCapabilitiesResponse response;
        if (login) soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", login, passwd);

        soapRes = soapProxy.GetCapabilities(endpoint.toStdString().c_str(), NULL, &request, &response);
        if (soapRes != SOAP_OK && cl_log.logLevel() >= cl_logDEBUG1) {
            qDebug() << "OnvifGeneric211ResourceSearcher::processPacket: can't fetch media and device URLs. Reason: SOAP to endpoint "
                     << endpoint << " failed. Error code: " << soapRes << "Description: "
                     << soapProxy.soap_fault_string() << ". " << soapProxy.soap_fault_detail();
        }

        if (response.Capabilities) {
            mediaUrl = response.Capabilities->Media? response.Capabilities->Media->XAddr.c_str(): mediaUrl;
            deviceUrl = response.Capabilities->Device? response.Capabilities->Device->XAddr.c_str(): deviceUrl;
        }
    }

    QnNetworkResourcePtr resource( new QnPlOnvifGeneric211Resource() );
    resource->setTypeId(onvifTypeId);
    resource->setName(name);
    resource->setMAC(mac);
    if (login) {
        qDebug() << "OnvifGeneric211ResourceSearcher::processPacket: Setting login = " << login << ", password = " << passwd;
        resource->setAuth(login, passwd);
    }
    if (!mediaUrl.isEmpty()) {
        qDebug() << "OnvifGeneric211ResourceSearcher::processPacket: Setting mediaUrl = " << mediaUrl;
        resource->setParam(QnPlOnvifGeneric211Resource::MEDIA_URL_PARAM_NAME, mediaUrl, QnDomainDatabase);
    }
    if (!deviceUrl.isEmpty()) {
        qDebug() << "OnvifGeneric211ResourceSearcher::processPacket: Setting deviceUrl = " << deviceUrl;
        resource->setParam(QnPlOnvifGeneric211Resource::DEVICE_URL_PARAM_NAME, deviceUrl, QnDomainDatabase);
    }

    qDebug() << "OnvifGeneric211ResourceSearcher::processPacket: Found new camera: endpoint: " << endpoint
             << ", MAC: " << mac << ", name: " << name;

    return resource;
}

const bool OnvifGeneric211ResourceSearcher::isMacAlreadyExists(const QString& mac, const QnResourceList& resList) const
{
    foreach(QnResourcePtr res, resList) {
        QnNetworkResourcePtr netRes = res.dynamicCast<QnNetworkResource>();

        if (netRes->getMAC().toString() == mac) {
            return true;
        }
    }

    return false;
}

const QString OnvifGeneric211ResourceSearcher::fetchMacAddress(const _onvifDevice__GetNetworkInterfacesResponse& response,
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

    qDebug() << "OnvifGeneric211ResourceSearcher::fetchMacAddress: appropriate MAC address was not found for device with IP: "
             << senderIpAddress.toStdString().c_str();
    return someMacAddress.toUpper().replace(":", "-");
}

const QString OnvifGeneric211ResourceSearcher::fetchName(const _onvifDevice__GetDeviceInformationResponse& response) const
{
    return QString((response.Manufacturer + " - " + response.Model).c_str());
}

const QString OnvifGeneric211ResourceSearcher::fetchSerialConvertToMac(const _onvifDevice__GetDeviceInformationResponse& response) const
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

const QString OnvifGeneric211ResourceSearcher::generateRandomPassword() const
{
    QTime curTime(QTime::currentTime());
    qsrand((curTime.hour() + 1) * (curTime.minute() + 1));

    QString tmp;
    QString result;
    result += tmp.setNum(qrand());
    result += tmp.setNum(curTime.second());
    result += tmp.setNum(qrand());
    result += tmp.setNum(curTime.msec());

    qDebug() << "Random password is " << result;
    return result;
}
