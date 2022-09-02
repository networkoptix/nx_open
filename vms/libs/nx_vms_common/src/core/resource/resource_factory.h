// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

class NX_VMS_COMMON_API QnResourceFactory
{
public:
    virtual ~QnResourceFactory() = default;

    virtual QnMediaServerResourcePtr createServer() const;

    virtual QnLayoutResourcePtr createLayout() const;

    virtual QnResourcePtr createResource(
        const QnUuid &resourceTypeId, const QnResourceParams &params) = 0;
};
