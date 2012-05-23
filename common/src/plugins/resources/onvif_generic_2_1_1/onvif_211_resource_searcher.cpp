#ifdef WIN32
#include "openssl/evp.h"
#else
#include "evp.h"
#endif

#include "onvif_211_resource_searcher.h"
#include "core/resource/camera_resource.h"
#include "onvif_211_resource.h"
#include "Onvif.nsmap"
#include "soapDeviceBindingProxy.h"
#include "wsseapi.h"

const char* OnvifGeneric211ResourceSearcher::ONVIF_RT = "ONVIF";


OnvifGeneric211ResourceSearcher::OnvifGeneric211ResourceSearcher():
    onvifTypeId(qnResTypePool->getResourceTypeByName(ONVIF_RT)->getId())
{
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

QnNetworkResourcePtr OnvifGeneric211ResourceSearcher::processPacket(QnResourceList& result, QByteArray&, const QHostAddress& sender)
{
    QString endpoint(QnPlOnvifGeneric211Resource::createOnvifEndpointUrl(sender.toString()));
    if (endpoint.isEmpty()) {
        qDebug() << "OnvifGeneric211ResourceSearcher::processPacket: response packet was received, but appropriate URL was not found.";
        return QnNetworkResourcePtr(0);
    }

    DeviceBindingProxy soapProxy;
    soap_register_plugin(soapProxy.soap, soap_wsse);

    //soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", "root", "admin123");
    //soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", "admin", "admin");
    _onvifDevice__GetNetworkInterfaces request1;
    _onvifDevice__GetNetworkInterfacesResponse response1;
    if (int soapRes = soapProxy.GetNetworkInterfaces(endpoint.toStdString().c_str(), NULL, &request1, &response1) != SOAP_OK) {
        qDebug() << "OnvifGeneric211ResourceSearcher::processPacket: SOAP to endpoint '" << endpoint
                 << "' failed. Error code: " << soapRes << "Description: "
                 << soapProxy.soap_fault_string() << ". " << soapProxy.soap_fault_detail();
        return QnNetworkResourcePtr(0);
    }
    QString mac(fetchNormalizeMacAddress(response1, sender.toString(), result));
    if (mac.isEmpty()) {
        return QnNetworkResourcePtr(0);
    }

    //soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", "root", "admin123");
    //soap_wsse_add_UsernameTokenDigest(soapProxy.soap, "Id", "admin", "admin");
    _onvifDevice__GetDeviceInformation request2;
    _onvifDevice__GetDeviceInformationResponse response2;
    if (int soapRes = soapProxy.GetDeviceInformation(endpoint.toStdString().c_str(), NULL, &request2, &response2) != SOAP_OK) {
        qDebug() << "OnvifGeneric211ResourceSearcher::processPacket: SOAP to endpoint '" << endpoint
                 << "' failed. Error code: " << soapRes << "Description: "
                 << soapProxy.soap_fault_string() << ". " << soapProxy.soap_fault_detail();
        return QnNetworkResourcePtr(0);
    }
    QString name(fetchName(response2));
    if (name.isEmpty()) {
        qWarning() << "OnvifGeneric211ResourceSearcher::processPacket: can't fetch name of ONVIF device: endpoint: " << endpoint
                   << ", MAC: " << mac;
        name = "Unknown - " + mac;
    }

    QnNetworkResourcePtr resource( new QnPlOnvifGeneric211Resource() );
    resource->setTypeId(onvifTypeId);
    resource->setName(name);
    resource->setMAC(mac);

    qDebug() << "OnvifGeneric211ResourceSearcher::processPacket: Found new camera: endpoint: " << endpoint
             << ", MAC: " << mac << ", name: " << name;

    return resource;
}

const QString OnvifGeneric211ResourceSearcher::fetchNormalizeMacAddress(const _onvifDevice__GetNetworkInterfacesResponse& response,
        const QString& senderIpAddress, QnResourceList& result) const {
    QString mac(fetchMacAddress(response, senderIpAddress).toUpper().replace(":", "-"));
    if (mac.isEmpty()) {
        qDebug() << "OnvifGeneric211ResourceSearcher::fetchNormalizeMacAddress: MAC address was not found.";
        return mac;
    }

    foreach(QnResourcePtr res, result)
    {
        QnNetworkResourcePtr net_res = res.dynamicCast<QnNetworkResource>();

        if (net_res->getMAC().toString() == mac)
            return QString(); // already found;
    }

    return mac;
}

const QString OnvifGeneric211ResourceSearcher::fetchMacAddress(const _onvifDevice__GetNetworkInterfacesResponse& response,
        const QString& senderIpAddress) const {
    std::vector<class onvifXsd__NetworkInterface*> ifaces = response.NetworkInterfaces;
    std::vector<class onvifXsd__NetworkInterface*>::const_iterator ifacePtrIter = ifaces.begin();

    qDebug() << "Sender address: " << senderIpAddress;
    while (ifacePtrIter != ifaces.end()) {
        onvifXsd__NetworkInterface* ifacePtr = *ifacePtrIter;

        if (ifacePtr->Enabled && ifacePtr->IPv4->Enabled) {
            onvifXsd__IPv4Configuration* conf = ifacePtr->IPv4->Config;

            if (conf->DHCP) qDebug() << "Found address: " << conf->FromDHCP->Address.c_str();
            if (conf->DHCP && senderIpAddress == conf->FromDHCP->Address.c_str()) {
                return QString(ifacePtr->Info->HwAddress.c_str());
            }

            std::vector<class onvifXsd__PrefixedIPv4Address*> addresses = ifacePtr->IPv4->Config->Manual;
            std::vector<class onvifXsd__PrefixedIPv4Address*>::const_iterator addrPtrIter = addresses.begin();

            while (addrPtrIter != addresses.end()) {
                onvifXsd__PrefixedIPv4Address* addrPtr = *addrPtrIter;

                if (conf->DHCP) qDebug() << "Found address: " << addrPtr->Address.c_str();
                if (senderIpAddress == addrPtr->Address.c_str()) {
                    return QString(ifacePtr->Info->HwAddress.c_str());
                }

                ++addrPtrIter;
            }
        }

        ++ifacePtrIter;
    }

    //return QString("77:77:77:77:77:77");
    //return QString("33:33:33:33:33:33");
    //return QString("77:77:77:77:77:71");
    //return QString("77:77:77:77:77:72");
    //return QString("77:77:77:77:77:73");
    return QString();
}

const QString OnvifGeneric211ResourceSearcher::fetchName(const _onvifDevice__GetDeviceInformationResponse& response) const {
    return QString((response.Manufacturer + " - " + response.Model).c_str());
}
