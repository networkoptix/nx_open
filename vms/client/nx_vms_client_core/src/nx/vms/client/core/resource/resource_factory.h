// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_factory.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API ResourceFactory: public QnResourceFactory
{
public:
    virtual QnMediaServerResourcePtr createServer() const override;
};

} // namespace nx::vms::client::core
