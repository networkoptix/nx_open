// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/resource/resource_factory.h>

namespace nx::vms::client::desktop {

class ResourceFactory: public core::ResourceFactory
{
public:
    virtual QnMediaServerResourcePtr createServer() const override;

    virtual QnResourcePtr createResource(
        const QnUuid& resourceTypeId,
        const QnResourceParams& params) override;

    virtual QnLayoutResourcePtr createLayout() const override;
};

} // namespace nx::vms::client::desktop
