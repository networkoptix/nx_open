#ifndef COMMON_APPSERVER_H
#define COMMON_APPSERVER_H

#include "core/resourcemanagment/resource_searcher.h"

class QnAppServerResourceSearcher : virtual public QnAbstractResourceSearcher
{
public:
    QnAppServerResourceSearcher();

    static QnAppServerResourceSearcher& instance();

    virtual QString manufacture() const;

    virtual QnResourceList findResources();

    virtual bool isResourceTypeSupported(QnId resourceTypeId) const;
    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);
};

#endif // COMMON_APPSERVER_H
