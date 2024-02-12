// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/user_data.h>

namespace ec2 {

class AbstractPersistentStorage
{
public:
    ~AbstractPersistentStorage() {}

    virtual bool isServer(const nx::Uuid&) = 0;
};

} // namespace ec2
