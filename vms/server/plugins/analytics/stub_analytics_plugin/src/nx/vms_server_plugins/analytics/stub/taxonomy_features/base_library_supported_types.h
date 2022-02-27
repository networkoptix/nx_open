// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace taxonomy_features {

static const std::string kBaseLibrarySupportedTypes = /*suppress newline*/ 1 + (const char*) R"json(
        {
            "attributes":
            [
                "Color",
                "Speed",
                "Brand",
                "Model",
                "Size",
                "License Plate",
                "Driver buckled up",
                "Lane"
            ],
            "objectTypeId":"nx.base.Vehicle"
        },
        {
            "attributes":
            [
                "Gender",
                "Complexion",
                "Age",
                "Height",
                "Activity",
                "Hat",
                "Scarf",
                "Body Shape",
                "Top Clothing Color",
                "Top Clothing Length",
                "Top Clothing Grain",
                "Top Clothing Type",
                "Bottom Clothing Color",
                "Bottom Clothing Length",
                "Bottom Clothing Grain",
                "Bottom Clothing Type",
                "Things",
                "Gloves",
                "Shoes",
                "Name",
                "Temperature",
                "Tattoo"
            ],
            "objectTypeId":"nx.base.Person"
        },
        {
            "attributes":
            [
                "Gender",
                "Complexion",
                "Age",
                "Shape",
                "Length",
                "Emotion",
                "Hat",
                "Hair Color",
                "Hair Type",
                "Eyelid",
                "Eyebrow Width",
                "Eyebrow Space",
                "Eyebrow Color",
                "Eyes",
                "Mouth",
                "Eyes Shape",
                "Eyes Color",
                "Nose Length",
                "Nose Bridge",
                "Nose Wing",
                "Nose End",
                "Facial Hair",
                "Ear Type",
                "Lip Type",
                "Chin Type",
                "Freckles",
                "Tattoo",
                "Mole",
                "Scar",
                "Things",
                "Temperature",
                "Name"
            ],
            "objectTypeId": "nx.base.Face"
        },
        {
            "attributes":
            [
                "Number",
                "Country",
                "State/Province",
                "Size",
                "Color"
            ],
            "objectTypeId": "nx.base.LicensePlate"
        },
        {
            "attributes": [ "Size", "Color" ],
            "objectTypeId": "nx.base.Animal"
        },
        {
            "attributes":
            [
                "Bag",
                "Weapon",
                "Cigarette",
                "Mobile Phone",
                "Cart",
                "Bottle",
                "Umbrella",
                "Box",
                "Mask",
                "Glasses",
                "Helmet"
            ],
            "objectTypeId": "nx.base.Things"
        },
        {
            "objectTypeId": "nx.base.Unknown"
        },
        {
            "attributes": [ "Type" ],
            "objectTypeId": "nx.base.Car"
        },
        {
            "attributes": [ "Type" ],
            "objectTypeId": "nx.base.Truck"
        },
        {
            "attributes": [ "Type" ],
            "objectTypeId": "nx.base.Bus"
        },
        {
            "objectTypeId": "nx.base.Train"
        },
        {
            "objectTypeId": "nx.base.Tram"
        },
        {
            "attributes": [ "Type" ],
            "objectTypeId": "nx.base.Bike"
        },
        {
            "attributes": [ "Type" ],
            "objectTypeId": "nx.base.Special"
        },
        {
            "objectTypeId": "nx.base.WaterTransport"
        },
        {
            "objectTypeId": "nx.base.AirTransport"
        },
        {
            "objectTypeId": "nx.base.Cat"
        },
        {
            "objectTypeId": "nx.base.Dog"
        },
        {
            "objectTypeId": "nx.base.Fish"
        },
        {
            "objectTypeId": "nx.base.Snake"
        },
        {
            "objectTypeId": "nx.base.Bird"
        },
        {
            "attributes": [ "Type" ],
            "objectTypeId": "nx.base.FacialHair"
        },
        {
            "attributes": [ "Color", "Type" ],
            "objectTypeId": "nx.base.Hat"
        },
        {
            "attributes": [ "Color" ],
            "objectTypeId": "nx.base.Scarf"
        },
        {
            "attributes": [ "Color" ],
            "objectTypeId": "nx.base.Gloves"
        },
        {
            "attributes": [ "Color", "Type" ],
            "objectTypeId": "nx.base.Shoes"
        },
        {
            "attributes": [ "Type" ],
            "objectTypeId": "nx.base.Glasses"
        },
        {
            "objectTypeId": "nx.base.Mask"
        },
        {
            "objectTypeId": "nx.base.Helmet"
        },
        {
            "attributes": [ "Color", "Open" ],
            "objectTypeId": "nx.base.Umbrella"
        },
        {
            "attributes": [ "Color", "Lug" ],
            "objectTypeId": "nx.base.Box"
        },
        {
            "objectTypeId": "nx.base.Weapon"
        },
        {
            "attributes": [ "Type" ],
            "objectTypeId": "nx.base.Cigarette"
        },
        {
            "attributes": [ "Position" ],
            "objectTypeId": "nx.base.MobilePhone"
        },
        {
            "attributes": [ "Type" ],
            "objectTypeId": "nx.base.Cart"
        },
        {
            "objectTypeId": "nx.base.Bottle"
        },
        {
            "attributes": [ "Size", "Color", "Type" ],
            "objectTypeId": "nx.base.Bag"
        },)json";

} // namespace taxonomy_features
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
