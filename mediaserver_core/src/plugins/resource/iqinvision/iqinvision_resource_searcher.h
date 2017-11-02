#pragma once
#if defined(ENABLE_IQE)

#include <core/resource_management/resource_searcher.h>
#include "../mdns/mdns_resource_searcher.h"

class QnPlIqResourceSearcher: public QnMdnsResourceSearcher
{
public:
    QnPlIqResourceSearcher(QnCommonModule* commonModule);

    virtual QnResourcePtr createResource(
        const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    /** @return Manufacturer of the server. */
    virtual QString manufacture() const;

    virtual QList<QnResourcePtr> checkHostAddr(
        const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

    static bool isIqeModel(const QString& model);

protected:
    virtual QList<QnNetworkResourcePtr> processPacket(
        QnResourceList& result,
        const QByteArray& responseData,
        const QHostAddress& discoveryAddress,
        const QHostAddress& foundHostAddress) override;

    virtual QnResourceList findResources() override;

private:
    void processNativePacket(QnResourceList& result, const QByteArray& responseData);
};

#endif // defined(ENABLE_IQE)
