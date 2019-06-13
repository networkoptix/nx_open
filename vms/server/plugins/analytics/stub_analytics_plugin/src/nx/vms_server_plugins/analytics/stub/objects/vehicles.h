#pragma once

#include "abstract_object.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

const std::string kCarObjectType = "nx.stub.car";
const std::string kTruckObjectType = "nx.stub.truck";

class Vehicle: public AbstractObject
{
    using base_type = AbstractObject;
public:
    Vehicle(const std::string& typeId, Attributes attributes);
    virtual void update() override;
};

class Car: public Vehicle
{
public:
    Car();
};

class Truck: public Vehicle
{
public:
    Truck();
};

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx