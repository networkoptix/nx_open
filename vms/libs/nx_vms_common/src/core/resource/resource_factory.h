// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/log/assert.h>
#include <nx/vms/api/data/user_data.h>
#include <utils/common/id.h>

#include "resource_fwd.h"

namespace nx::vms::common { class SystemContext; }

struct QnResourceParams
{
    QnResourceParams() = default;
    QnResourceParams(nx::Uuid resID, QString url, QString vendor) noexcept:
        resID(std::move(resID)),
        url(std::move(url)),
        vendor(std::move(vendor))
    {
        NX_ASSERT(!resID.isNull());
    }

    nx::Uuid resID;
    QString url;
    QString vendor;
};

class NX_VMS_COMMON_API QnResourceFactory
{
public:
    virtual ~QnResourceFactory() = default;

    virtual QnMediaServerResourcePtr createServer() const;

    virtual QnLayoutResourcePtr createLayout() const;

    virtual QnUserResourcePtr createUser(
        nx::vms::common::SystemContext* systemContext,
        const nx::vms::api::UserData& data) const;

    virtual QnResourcePtr createResource(
        const nx::Uuid &resourceTypeId, const QnResourceParams &params) = 0;
};
