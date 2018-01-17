#ifndef axis_device_server_h_2219
#define axis_device_server_h_2219

#ifdef ENABLE_AXIS

#include "core/resource_management/resource_searcher.h"
#include "../mdns/mdns_resource_searcher.h"


class QnPlAxisResourceSearcher : public QnMdnsResourceSearcher
{
    using base_type = QnMdnsResourceSearcher;
public:
    QnPlAxisResourceSearcher(QnCommonModule* commonModule);

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    // return the manufacture of the server
    virtual QString manufacture() const override;

    virtual QList<QnResourcePtr> checkHostAddr(const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

protected:
    QList<QnNetworkResourcePtr> processPacket(
        QnResourceList& result,
        const QByteArray& responseData,
        const QHostAddress& discoveryAddress,
        const QHostAddress& foundHostAddress ) override;

private:
    bool testCredentials(const QUrl& url, const QAuthenticator& auth) const;

    QAuthenticator determineResourceCredentials(const QnSecurityCamResourcePtr& resource) const;

    template<typename T>
    void addMultichannelResources(QList<T>& result);

};

#endif // #ifdef ENABLE_AXIS
#endif // axis_device_server_h_2219
