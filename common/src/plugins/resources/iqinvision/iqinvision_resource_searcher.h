#ifndef iq_device_server_h_1825
#define iq_device_server_h_1825

#ifdef ENABLE_IQE

#include "core/resource_management/resource_searcher.h"
#include "../mdns/mdns_device_searcher.h"


class QnPlIqResourceSearcher : public QnMdnsResourceSearcher
{
    QnPlIqResourceSearcher();

public:
    static QnPlIqResourceSearcher& instance();

    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParams& params);

    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

protected:
    virtual QList<QnNetworkResourcePtr> processPacket(QnResourceList& result, const QByteArray& responseData, const QHostAddress& discoveryAddress) override;
    virtual QnResourceList findResources() override;
private:
    void  processNativePacket(QnResourceList& result, const QByteArray& responseData, const QHostAddress& discoveryAddress);
};

#endif // #ifdef ENABLE_IQE
#endif //iq_device_server_h_1825
