#include "uplink_speed_tester_factory.h"

#include "scheduled_uplink_speed_tester.h"

namespace nx::network::cloud::speed_test {

UplinkSpeedTesterFactory::UplinkSpeedTesterFactory():
    BasicFactory(std::bind(&UplinkSpeedTesterFactory::defaultFactoryFunc, this))
{
}

UplinkSpeedTesterFactory& UplinkSpeedTesterFactory::instance()
{
    static UplinkSpeedTesterFactory factory;
    return factory;
}

std::unique_ptr<AbstractSpeedTester> UplinkSpeedTesterFactory::defaultFactoryFunc()
{
    return std::make_unique<ScheduledUplinkSpeedTester>();
}

} //namespace nx::network::cloud::speed_test
