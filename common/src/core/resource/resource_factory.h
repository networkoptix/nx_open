#ifndef QN_RESOURCE_FACTORY_H
#define QN_RESOURCE_FACTORY_H

#include "resource_fwd.h"

#include <utils/common/id.h>

struct QnResourceParams {
    QnResourceParams() {}
    QnResourceParams(const QString &url, const QString &vendor): url(url), vendor(vendor) {}

    QString url;
    QString vendor;
};


class QnResourceFactory {
public:
    virtual ~QnResourceFactory() {}

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams &params) = 0;
};

#endif // QN_RESOURCE_FACTORY_H
