#pragma once

#ifdef ENABLE_PULSE_CAMERA

#include "core/resource_management/resource_searcher.h"


class QnPlPulseSearcher: public QnAbstractNetworkResourceSearcher
{
    QnPlPulseSearcher(QnCommonModule* commonModule);

public:
    QnResourceList findResources(void);

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QnResourceList checkEndpoint(
        const QUrl& url, const QAuthenticator& auth,
        const QString& physicalId, QnResouceSearchMode mode) override;

protected:
    // return the manufacture of the server
    virtual QString manufacture() const;

private:

    QnNetworkResourcePtr createResource(const QString& manufacture, const QString& name);

};

#endif // #ifdef ENABLE_PULSE_CAMERA
