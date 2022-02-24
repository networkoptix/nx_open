// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "shared_memory_manager.h"

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

/** Implementation of the shared memory interface based on the QSharedMemory class. */
class NX_VMS_CLIENT_DESKTOP_API QtBasedSharedMemory: public SharedMemoryInterface
{
public:
    /** Create shared memory using the provided id. Default value is used if empty. */
    explicit QtBasedSharedMemory(const QString& explicitId = QString());
    virtual ~QtBasedSharedMemory() override;

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
