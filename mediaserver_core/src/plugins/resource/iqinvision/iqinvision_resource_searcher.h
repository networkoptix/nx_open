#ifndef iq_device_server_h_1825
#define iq_device_server_h_1825

#ifdef ENABLE_IQE

#include "core/resource_management/resource_searcher.h"
#include "../mdns/mdns_resource_searcher.h"


class QnPlIqResourceSearcher : public QnMdnsResourceSearcher
{

public:
    QnPlIqResourceSearcher();

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

protected:
    virtual QList<QnNetworkResourcePtr> processPacket(
        QnResourceList& result,
        const QByteArray& responseData,
        const QHostAddress& discoveryAddress,
        const QHostAddress& foundHostAddress ) override;
    virtual QnResourceList findResources() override;
private:
    void  processNativePacket(QnResourceList& result, const QByteArray& responseData);
};

#endif // #ifdef ENABLE_IQE
#endif //iq_device_server_h_1825
