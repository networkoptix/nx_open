#ifndef axis_device_server_h_2219
#define axis_device_server_h_2219

#ifdef ENABLE_AXIS

#include "core/resource_management/resource_searcher.h"
#include "../mdns/mdns_resource_searcher.h"


class QnPlAxisResourceSearcher : public QnMdnsResourceSearcher
{

public:
    QnPlAxisResourceSearcher();

    virtual QnResourcePtr createResource(const QUuid &resourceTypeId, const QnResourceParams& params) override;

    // return the manufacture of the server
    virtual QString manufacture() const override;

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
private:
    template <class T> void addMultichannelResources(QList<T>& result);
protected:
    QList<QnNetworkResourcePtr> processPacket(
        QnResourceList& result,
        const QByteArray& responseData,
        const QHostAddress& discoveryAddress,
        const QHostAddress& foundHostAddress ) override;
};

#endif // #ifdef ENABLE_AXIS
#endif // axis_device_server_h_2219
