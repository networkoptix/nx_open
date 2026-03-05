// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::vms::client::core {

class VmsDbDataLoader;

class NX_VMS_CLIENT_CORE_API VmsDbDataProcessor
{
public:
    void onDataReady(const VmsDbDataLoader* loader);
};

} // namespace nx::vms::client::core
