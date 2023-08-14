// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_factory.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API ResourceFactory: public QnResourceFactory
{
    using base_type = QnResourceFactory;

public:
    virtual QnMediaServerResourcePtr createServer() const override;

    virtual QnUserResourcePtr createUser(
        nx::vms::common::SystemContext* systemContext,
        const nx::vms::api::UserData& data) const override;
};

} // namespace nx::vms::client::core
