#pragma once

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include <nx/sdk/helpers/attribute.h>
#include <nx/sdk/helpers/ptr.h>

#include "../vector2.h"
#include "random_state_changer.h"
#include "random_pauser.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

const std::string kCarObjectType = "nx.stub.car";
const std::string kHumanFaceObjectType = "nx.stub.humanFace";
const std::string kTruckObjectType = "nx.stub.truck";
const std::string kPedestrianObjectType = "nx.stub.pedestrian";
const std::string kBicycleObjectType = "nx.stub.bicycle";

using Attributes = std::vector<nx::sdk::Ptr<nx::sdk::Attribute>>;

class AbstractObject
{
public:
    AbstractObject(const std::string& typeId, Attributes attributes);
    ~AbstractObject() = default;

    Vector2 position() const;
    Size size() const;
    Attributes attributes() const;
    nx::sdk::Uuid id() const;
    std::string typeId() const;

    bool inBounds() const;

    virtual void update() = 0;

protected:
    std::string m_typeId;
    Attributes m_attributes;
    nx::sdk::Uuid m_id;
    Vector2 m_position;
    Size m_size;
    float m_speed = 0;
    Vector2 m_trajectory;
};

//-------------------------------------------------------------------------------------------------

class Bicycle : public AbstractObject
{
    using base_type = AbstractObject;
public:
    Bicycle();
    virtual void update() override;

private:
    RandomStateChanger m_stateChanger;
};

//-------------------------------------------------------------------------------------------------

class Vehicle : public AbstractObject
{
    using base_type = AbstractObject;
public:
    Vehicle(const std::string& typeId, Attributes attributes);
    virtual void update() override;
};

class Car : public Vehicle
{
public:
    Car();
};

class Truck : public Vehicle
{
public:
    Truck();
};

//-------------------------------------------------------------------------------------------------

class Pedestrian : public AbstractObject
{
    using base_type = AbstractObject;
public:
    Pedestrian();

    virtual void update() override;

private:
    RandomPauser m_pauser;
    RandomStateChanger m_stateChanger;
};

//-------------------------------------------------------------------------------------------------

class HumanFace : public AbstractObject
{
    using base_type = AbstractObject;
public:
    HumanFace();
    virtual void update() override;

private:
    RandomPauser m_pauser;
};

//-------------------------------------------------------------------------------------------------

float randomDecimal(float min = 0, float max = 1);

Vector2 randomTrajectory();

std::unique_ptr<AbstractObject> randomObject();

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx