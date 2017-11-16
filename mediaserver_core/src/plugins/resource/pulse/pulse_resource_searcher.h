#pragma once

#ifdef ENABLE_PULSE_CAMERA

#include "core/resource_management/resource_searcher.h"


class QnPlPulseSearcher: public QnAbstractNetworkResourceSearcher
{
    QnPlPulseSearcher(QnCommonModule* commonModule);

public:
    QnResourceList findResources(void);

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QList<QnResourcePtr> checkHostAddr(const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

protected:
    // return the manufacture of the server
    virtual QString manufacture() const;

private:

    QnNetworkResourcePtr createResource(const QString& manufacture, const QString& name);

};

#endif // #ifdef ENABLE_PULSE_CAMERA
