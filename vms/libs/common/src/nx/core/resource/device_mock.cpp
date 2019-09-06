#include "device_mock.h"

namespace nx::core::resource {

QnAbstractStreamDataProvider* DeviceMock::createLiveDataProvider()
{
    return nullptr;
}

} // namespace nx::core::resource
