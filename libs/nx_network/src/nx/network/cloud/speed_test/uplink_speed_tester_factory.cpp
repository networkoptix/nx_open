// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "uplink_speed_tester_factory.h"

#include "uplink_speed_tester.h"

namespace nx::network::cloud::speed_test {

UplinkSpeedTesterFactory::UplinkSpeedTesterFactory():
    BasicFactory(
        std::bind(&UplinkSpeedTesterFactory::defaultFactoryFunc, this, std::placeholders::_1))
{
}

UplinkSpeedTesterFactory& UplinkSpeedTesterFactory::instance()
{
    static UplinkSpeedTesterFactory factory;
    return factory;
}

std::unique_ptr<AbstractSpeedTester> UplinkSpeedTesterFactory::defaultFactoryFunc(
    const AbstractSpeedTester::Settings& settings)
{
    return std::make_unique<UplinkSpeedTester>(settings);
}

} //namespace nx::network::cloud::speed_test
