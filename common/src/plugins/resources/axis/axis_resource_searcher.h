#ifndef axis_device_server_h_2219
#define axis_device_server_h_2219

#ifdef ENABLE_AXIS

#include "core/resource_management/resource_searcher.h"
#include "../mdns/mdns_device_searcher.h"


class QnPlAxisResourceSearcher : public QnMdnsResourceSearcher
{
    QnPlAxisResourceSearcher();

public:
    static QnPlAxisResourceSearcher& instance();

    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParams& params) override;

    // return the manufacture of the server
    virtual QString manufacture() const override;

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

protected:
    QList<QnNetworkResourcePtr> processPacket(QnResourceList& result, const QByteArray& responseData, const QHostAddress& discoveryAddress) override;
};

#endif // #ifdef ENABLE_AXIS
#endif // axis_device_server_h_2219
