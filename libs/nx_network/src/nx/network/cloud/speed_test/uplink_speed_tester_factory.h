#pragma once

#include <nx/utils/basic_factory.h>

#include "abstract_speed_tester.h"

namespace nx::network::cloud::speed_test {

using UplinkSpeedTesterFactoryType = 
    std::unique_ptr<AbstractSpeedTester>(const AbstractSpeedTester::Settings&);

class NX_NETWORK_API UplinkSpeedTesterFactory:
    public nx::utils::BasicFactory<UplinkSpeedTesterFactoryType>
{
public:
    UplinkSpeedTesterFactory();

    static UplinkSpeedTesterFactory& instance();

private:
    std::unique_ptr<AbstractSpeedTester> defaultFactoryFunc(
        const AbstractSpeedTester::Settings& settings);
};

}
