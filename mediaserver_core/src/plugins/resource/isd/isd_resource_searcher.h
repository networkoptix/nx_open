#ifndef isd_device_server_h_1936
#define isd_device_server_h_1936

#ifdef ENABLE_ISD

#include "core/resource_management/resource_searcher.h"
#include "../mdns/mdns_resource_searcher.h"
#include "../upnp/upnp_resource_searcher.h"


class QnPlISDResourceSearcher : public QnUpnpResourceSearcherAsync
{

public:
    QnPlISDResourceSearcher();

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

protected:

    virtual void processPacket(
            const QHostAddress& discoveryAddr,
            const HostAddress& host,
            const UpnpDeviceInfo& devInfo,
            const QByteArray& xmlDevInfo,
            QnResourceList& result) override;

private:

    void createResource(
        const UpnpDeviceInfo& devInfo,
        const QnMacAddress& mac,
        const QAuthenticator& auth,
        QnResourceList& result );

};

#endif // #ifdef ENABLE_ISD
#endif //isd_device_server_h_1936
