#pragma once

#ifdef ENABLE_DROID

#include "core/resource_management/resource_searcher.h"

class QnPlIpWebCamResourceSearcher: public QnAbstractNetworkResourceSearcher
{
    using base_type = QnAbstractNetworkResourceSearcher;
public:
    QnPlIpWebCamResourceSearcher(QnCommonModule* commonModule);

    QnResourceList findResources();

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QList<QnResourcePtr> checkHostAddr(const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

protected:
    // return the manufacture of the server
    virtual QString manufacture() const;
};

#endif // #ifdef ENABLE_DROID
