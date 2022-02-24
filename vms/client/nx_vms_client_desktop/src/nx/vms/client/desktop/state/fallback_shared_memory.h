// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "shared_memory_manager.h"

namespace nx::vms::client::desktop {

/** Dummy local implementation of the shared memory interface. */
class FallbackSharedMemory: public SharedMemoryInterface
{
public:
    virtual bool initialize() override;

    virtual void lock() override;
    virtual void unlock() override;

    virtual SharedMemoryData data() const override;
    virtual void setData(const SharedMemoryData& data) override;

private:
    SharedMemoryData m_data;
};

} //namespace nx::vms::client::desktop
