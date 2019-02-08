#pragma once

#include "resource_fwd.h"

#include <utils/common/id.h>
#include <nx/utils/log/assert.h>

struct QnResourceParams
{
    QnResourceParams() = default;
    QnResourceParams(QnUuid resID, QString url, QString vendor) noexcept:
        resID(std::move(resID)),
        url(std::move(url)),
        vendor(std::move(vendor))
    {
        NX_ASSERT(!resID.isNull());
    }

    QnUuid resID;
    QString url;
    QString vendor;
};

class QnResourceFactory
{
public:
    virtual ~QnResourceFactory() = default;

    virtual QnResourcePtr createResource(
        const QnUuid &resourceTypeId, const QnResourceParams &params) = 0;
};
