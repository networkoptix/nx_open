#pragma once

#include <nx/utils/basic_factory.h>

#include "abstract_speed_tester.h"

namespace nx::network::cloud::speed_test {

class NX_NETWORK_API UplinkSpeedTesterFactory:
    public nx::utils::BasicFactory<std::unique_ptr<AbstractSpeedTester>(void)>
{
public:
    UplinkSpeedTesterFactory();

    static UplinkSpeedTesterFactory& instance();

private:
    std::unique_ptr<AbstractSpeedTester> defaultFactoryFunc();
};

}
