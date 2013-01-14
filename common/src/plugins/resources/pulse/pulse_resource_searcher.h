#ifndef pulse_resource_searcher_h
#define pulse_resource_searcher_h

#include "core/resource_managment/resource_searcher.h"


class QnPlPulseSearcher : public QnAbstractNetworkResourceSearcher
{
    QnPlPulseSearcher();

public:
    static QnPlPulseSearcher& instance();

    QnResourceList findResources(void);

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

protected:
    // return the manufacture of the server
    virtual QString manufacture() const;

private:
    
    QnNetworkResourcePtr createResource(const QString& manufacture, const QString& name);
    
};

#endif // pulse
