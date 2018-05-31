#pragma once

#include "core/resource_management/resource_searcher.h"

class QnPlC2pCameraResourceSearcher : public QnAbstractNetworkResourceSearcher
{
public:
    QnPlC2pCameraResourceSearcher(QnCommonModule* commonModule);

    virtual QnResourceList findResources(void) override;

    virtual QnResourcePtr createResource(
        const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QList<QnResourcePtr> checkHostAddr(
        const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

protected:
    // return the manufacture of the server
    virtual QString manufacture() const override;

};
