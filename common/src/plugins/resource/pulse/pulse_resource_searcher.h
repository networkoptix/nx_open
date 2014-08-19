#ifndef pulse_resource_searcher_h
#define pulse_resource_searcher_h

#ifdef ENABLE_PULSE_CAMERA

#include "core/resource_management/resource_searcher.h"


class QnPlPulseSearcher : public QnAbstractNetworkResourceSearcher
{
    QnPlPulseSearcher();

public:
    static QnPlPulseSearcher& instance();

    QnResourceList findResources(void);

    virtual QnResourcePtr createResource(const QUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

protected:
    // return the manufacture of the server
    virtual QString manufacture() const;

private:
    
    QnNetworkResourcePtr createResource(const QString& manufacture, const QString& name);
    
};

#endif // #ifdef ENABLE_PULSE_CAMERA
#endif // pulse
