#ifndef COMMON_APPSERVER_H
#define COMMON_APPSERVER_H

#include "core/resourcemanagment/resourceserver.h"

class QnAppServerResourceSearcher : virtual public QnAbstractResourceSearcher
{
public:
    QnAppServerResourceSearcher();

    static QnAppServerResourceSearcher& instance();

    virtual QString manufacture() const;

    virtual QnResourceList findResources();

    virtual QnResourcePtr createResource(const QnId& resourceTypeId, const QnResourceParameters& parameters);
    virtual bool isResourceTypeSupported(const QnId& resourceTypeId) const;

private:
    bool m_isFirstTime;
};

#endif // COMMON_APPSERVER_H
