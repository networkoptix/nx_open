// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vehicles.h"

#include <string>
#include <map>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

using namespace nx::sdk;

namespace {

using VehicleMap = std::map<const char*, std::vector<const char*>>;

static const VehicleMap kCars = {
    {"Honda", {"Civic", "Accord"}},
    {"Toyota", {"Corolla", "Camry", "Supra"}},
    {"Ford", {"Mustang", "Focus", "GT"}},
    {"Lada", {"Granta", "Vesta", "Largus"}},
    {"Mazda", {"Miata", "3", "6", "Cx3"}},
    {"Subaru", {"Impreza", "Wrx", "Legacy", "Crosstrek", "Outback"}},
    {"Tesla", {"Model 3", "Model S", "Model X"}}
};

static const VehicleMap kTrucks = {
    {"Honda", {"Ridgeline"}},
    {"Toyota", {"Tacoma", "Tundra"}},
    {"Ford", {"F150", "F250", "F350"}},
    {"Lada", {"4x4 Bronco"}},
    {"Nissan", {"Frontier", "Titan"}},
    {"GMC", {"Canyon", "Sierra"}},
    {"Chevy", {"Colorado", "Silverado"}}
};

static const char* randomColor()
{
    static const std::vector<const char*> kColors = {
        "Red", "Orange", "Yellow", "Green", "Blue", "Indigo", "Violet", "Pink", "Purple", "White",
        "Black", "Grey"
    };
    return kColors[rand() % kColors.size()];
}

static std::pair<const char*, const char*> randomVehicle(const VehicleMap& map)
{
    auto it = map.begin();
    std::advance(it, rand() % map.size());
    return std::make_pair(it->first, it->second[rand() % it->second.size()]);
}

/** @param makeDistanceAttribute Make an attribute that will change its value for every frame. */
static Attributes makeAttributes(
    const VehicleMap& map, bool makeDistanceAttribute = false)
{
    const auto vehicle = randomVehicle(map);
    Attributes attributes = {
        makePtr<Attribute>(IAttribute::Type::string, "Brand", vehicle.first),
        makePtr<Attribute>(IAttribute::Type::string, "Model", vehicle.second),
        makePtr<Attribute>(IAttribute::Type::string, "Color", randomColor()),
    };
    if (makeDistanceAttribute)
    {
        attributes.push_back(makePtr<Attribute>(
            IAttribute::Type::number, "Distance", "UNDEFINED"));
    }
    return attributes;
}

} // namespace

Vehicle::Vehicle(const std::string& typeId, Attributes attributes):
    base_type(typeId, std::move(attributes))
{
}

void Vehicle::updateDistanceAttributeIfItExists()
{
    for (const auto& attribute: m_attributes)
    {
        if (attribute->name() == std::string("Distance"))
        {
            attribute->setValue(std::to_string(m_position.magnitude()));
            break;
        }
    }
}

void Vehicle::update()
{
    m_position += m_trajectory * m_speed;
    updateDistanceAttributeIfItExists();
}

Car::Car():
    Vehicle(kCarObjectType, makeAttributes(kCars, /*makeDistanceAttribute*/ true))
{
    updateDistanceAttributeIfItExists();
}

Truck::Truck():
    Vehicle(kTruckObjectType, makeAttributes(kTrucks))
{
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
