// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "shared_memory_manager.h"

namespace nx::vms::client::desktop {

/** Implementation of the shared memory interface based on the memory mapped file. */
class NX_VMS_CLIENT_DESKTOP_API FileBasedSharedMemory: public SharedMemoryInterface
{
public:
    /** Create shared memory using the provided id. Default value is used if empty. */
    explicit FileBasedSharedMemory(const QString& explicitId = QString());
    virtual ~FileBasedSharedMemory() override;

    virtual bool initialize() override;

    virtual void lock() override;
    virtual void unlock() override;

    virtual SharedMemoryData data() const override;
    virtual void setData(const SharedMemoryData& data) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} //namespace nx::vms::client::desktop
