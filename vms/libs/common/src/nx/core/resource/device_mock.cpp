#include "device_mock.h"

namespace nx::core::resource {

QnAbstractStreamDataProvider* DeviceMock::createLiveDataProvider()
{
    return nullptr;
}

bool DeviceMock::saveProperties()
{
    return true;
}

} // namespace nx::core::resource
