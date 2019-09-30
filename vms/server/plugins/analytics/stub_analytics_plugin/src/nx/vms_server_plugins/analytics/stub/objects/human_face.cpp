// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "human_face.h"

#include "random.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

using namespace nx::sdk;

namespace {

static std::pair<const char*, const char*> randomPerson()
{
    static const std::map<const char*, std::vector<const char*>> kPeople = {
        {"Female", {"Kat", "Olga", "Triss", "Judy", "Nate"}},
        {"Male", {
            "Andrey", "Ivan", "Roman", "Nick", "Sergey", "Oleg", "Yingfan", "Tony", "Igal",
            "Sasha", "Evgeny", "Kyle", "Borris", "Mike"}}
    };

    auto it = kPeople.begin();
    std::advance(it, rand() % kPeople.size());
    return std::make_pair(it->first, it->second[rand() % it->second.size()]);
}

static Attributes makeAttributes()
{
    static const std::vector<const char*> kHairColors = {
        "Red", "Brown", "Blonde", "Black", "Blue"
    };

    const auto person = randomPerson();
    return {
        nx::sdk::makePtr<Attribute>(IAttribute::Type::string, "Sex", person.first),
        nx::sdk::makePtr<Attribute>(IAttribute::Type::string, "Name", person.second),
        nx::sdk::makePtr<Attribute>(
            IAttribute::Type::string,
            "Hair color",
            kHairColors[rand() % kHairColors.size()]),
        nx::sdk::makePtr<Attribute>(IAttribute::Type::string, "Age", std::to_string(rand() % 50))
    };
}

} // namespace

 HumanFace::HumanFace():
     base_type(kHumanFaceObjectType, makeAttributes())
 {
 }

 void HumanFace::update()
 {
     m_pauser.update();
     if (m_pauser.paused())
         return;

     if (m_pauser.resuming())
         m_trajectory = randomTrajectory();

     m_position += m_trajectory * m_speed;
 }

 } // namespace stub
 } // namespace analytics
 } // namespace vms_server_plugins
 } // namespace nx