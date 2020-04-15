#pragma once

#include <nx/sdk/ptr.h>

namespace nx::vms_server_plugins::analytics::vivotek {

template <typename T>
nx::sdk::Ptr<T> addPtrRef(T* rawPtr)
{
    rawPtr->addRef();
    nx::sdk::Ptr<T> ptr;
    ptr.reset(rawPtr);
    return ptr;
}

} // namespace nx::vms_server_plugins::analytics::vivotek
