#pragma once

#include "core/resource_management/resource_searcher.h"

class QnWearableCameraResourceSearcher: public QnAbstractNetworkResourceSearcher
{
    using base_type = QnAbstractNetworkResourceSearcher;
public:
    QnWearableCameraResourceSearcher(QnCommonModule* commonModule);
    virtual ~QnWearableCameraResourceSearcher() override;

    virtual QString manufacture() const override;

    virtual bool isResourceTypeSupported(QnUuid resourceTypeId) const override;

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QnResourceList findResources() override;

    virtual QnResourceList checkEndpoint(
        const QUrl& url, const QAuthenticator& auth,
        const QString& physicalId, QnResouceSearchMode mode) override;
};

