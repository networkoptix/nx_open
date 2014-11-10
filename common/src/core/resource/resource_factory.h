#ifndef QN_RESOURCE_FACTORY_H
#define QN_RESOURCE_FACTORY_H

#include "resource_fwd.h"

#include <utils/common/id.h>

struct QnResourceParams {
    QnResourceParams() {}
    QnResourceParams(
        const QnUuid &resID,
        const QString &url,
        const QString &vendor)
    :
        resID(resID),
        url(url),
        vendor(vendor)
    {
        assert( !resID.isNull() );
    }

    QnUuid resID;
    QString url;
    QString vendor;
};


class QnResourceFactory {
public:
    virtual ~QnResourceFactory() {}

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams &params) = 0;
};

#endif // QN_RESOURCE_FACTORY_H
