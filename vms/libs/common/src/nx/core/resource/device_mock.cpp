#include "device_mock.h"

namespace nx::core::resource {

QString DeviceMock::getDriverName() const
{
    return "Device Mock";
}

QnAbstractStreamDataProvider* DeviceMock::createLiveDataProvider()
{
    return nullptr;
}

bool DeviceMock::saveProperties()
{
    return true;
}

} // namespace nx::core::resource
