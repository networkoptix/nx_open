#ifndef onvif_generic_2_1_1_resource_searcher_h
#define onvif_generic_2_1_1_resource_searcher_h

#include "core/resourcemanagment/resource_searcher.h"
#include "../onvif/onvif_device_searcher.h"
#include "onvif_211_helper.h"

class _onvifDevice__GetNetworkInterfacesResponse;
class _onvifDevice__GetDeviceInformationResponse;
class SOAP_ENV__Fault;

class OnvifGeneric211ResourceSearcher : public OnvifResourceSearcher
{
    OnvifGeneric211ResourceSearcher();

public:
    static OnvifGeneric211ResourceSearcher& instance();

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);
    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QnResourcePtr checkHostAddr(QHostAddress addr);

protected:
    QnNetworkResourcePtr processPacket(QnResourceList& result, QByteArray& responseData, const QHostAddress& sender);

private:
    static const char* ONVIF_RT;
    QnId onvifTypeId;
    PasswordHelper passHelper;

    const QString fetchMacAddress(const _onvifDevice__GetNetworkInterfacesResponse& response, const QString& senderIpAddress) const;
    const bool isMacAlreadyExists(const QString& mac, const QnResourceList& resList) const;
    const QString fetchName(const _onvifDevice__GetDeviceInformationResponse& response) const;
    const QString fetchSerialConvertToMac(const _onvifDevice__GetDeviceInformationResponse& response) const;
    const QString generateRandomPassword() const;
};

#endif // onvif_generic_2_1_1_resource_searcher
