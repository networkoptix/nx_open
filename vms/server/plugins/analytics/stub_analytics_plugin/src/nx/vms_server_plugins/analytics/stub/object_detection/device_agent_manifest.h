// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_detection {

static const std::string kDeviceAgentManifest = /*suppress newline*/ 1 + (const char*) R"json(
{
    "supportedTypes":
    [
        {
            "objectTypeId": "nx.base.Vehicle",
            "attributes":
            [
                "Color",
                "Speed",
                "Brand",
                "Model",
                "Size",
                "License Plate",
                "License Plate.Number",
                "License Plate.Country",
                "License Plate.State/Province",
                "License Plate.Size",
                "License Plate.Color",
                "Driver buckled up",
                "Lane"
            ]
        },
        {
            "objectTypeId": "nx.base.Person",
            "attributes":
            [
                "Gender",
                "Race",
                "Age",
                "Height",
                "Activity",
                "Hat",
                "Hat.Color",
                "Hat.Type",
                "Scarf",
                "Scarf.Color",
                "Body Shape",
                "Top Clothing Color",
                "Top Clothing Length",
                "Top Clothing Grain",
                "Top Clothing Type",
                "Bottom Clothing Color",
                "Bottom Clothing Length",
                "Bottom Clothing Grain",
                "Bottom Clothing Type",
                "Gloves",
                "Gloves.Color",
                "Shoes",
                "Shoes.Color",
                "Shoes.Type",
                "Name",
                "Temperature",
                "Tattoo",
                "Bag",
                "Bag.Size",
                "Bag.Color",
                "Bag.Type",
                "Weapon",
                "Cigarette",
                "Cigarette.Type",
                "Mobile Phone",
                "Mobile Phone.Position",
                "Cart",
                "Cart.Type",
                "Bottle",
                "Umbrella",
                "Umbrella.Color",
                "Umbrella.Open",
                "Box",
                "Box.Color",
                "Box.Lug",
                "Mask",
                "Glasses",
                "Glasses.Type",
                "Helmet"
            ]
        },
        {
            "objectTypeId": "nx.base.Face",
            "attributes":
            [
                "Gender",
                "Race",
                "Age",
                "Shape",
                "Length",
                "Emotion",
                "Hat",
                "Hat.Color",
                "Hat.Type",
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
                "Facial Hair.Type",
                "Ear Type",
                "Lip Type",
                "Chin Type",
                "Freckles",
                "Tattoo",
                "Mole",
                "Scar",
                "Temperature",
                "Name",
                "Cigarette",
                "Cigarette.Type",
                "Mask",
                "Glasses",
                "Glasses.Type",
                "Helmet"
            ]
        },
        {
            "objectTypeId": "nx.base.LicensePlate",
            "attributes":
            [
                "Number",
                "Country",
                "State/Province",
                "Size",
                "Color"
            ]
        },
        {
            "objectTypeId": "nx.base.Animal",
            "attributes":
            [
                "Size",
                "Color"
            ]
        },
        {
            "objectTypeId": "nx.base.Unknown",
            "attributes": []
        },
        {
            "objectTypeId": "nx.base.Car",
            "attributes":
            [
                "Type",
                "Color",
                "Speed",
                "Brand",
                "Model",
                "Size",
                "License Plate",
                "Driver buckled up",
                "Lane"
            ]
        },
        {
            "objectTypeId": "nx.base.Truck",
            "attributes":
            [
                "Type",
                "Color",
                "Speed",
                "Brand",
                "Model",
                "Size",
                "License Plate",
                "Driver buckled up",
                "Lane"
            ]
        },
        {
            "objectTypeId": "nx.base.Bus",
            "attributes":
            [
                "Type",
                "Color",
                "Speed",
                "Brand",
                "Model",
                "Size",
                "License Plate",
                "Driver buckled up",
                "Lane"
            ]
        },
        {
            "objectTypeId": "nx.base.Train",
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
            ]
        },
        {
            "objectTypeId": "nx.base.Tram",
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
            ]
        },
        {
            "objectTypeId": "nx.base.Bike",
            "attributes":
            [
                "Type",
                "Color",
                "Speed",
                "Brand",
                "Model",
                "Size",
                "License Plate",
                "Driver buckled up",
                "Lane"
            ]
        },
        {
            "objectTypeId": "nx.base.Special",
            "attributes":
            [
                "Type",
                "Color",
                "Speed",
                "Brand",
                "Model",
                "Size",
                "License Plate",
                "Driver buckled up",
                "Lane"
            ]
        },
        {
            "objectTypeId": "nx.base.WaterTransport",
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
            ]
        },
        {
            "objectTypeId": "nx.base.AirTransport",
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
            ]
        },
        {
            "objectTypeId": "nx.base.Cat",
            "attributes":
            [
                "Size",
                "Color"
            ]
        },
        {
            "objectTypeId": "nx.base.Dog",
            "attributes":
            [
                "Size",
                "Color"
            ]
        },
        {
            "objectTypeId": "nx.base.Fish",
            "attributes":
            [
                "Size",
                "Color"
            ]
        },
        {
            "objectTypeId": "nx.base.Snake",
            "attributes":
            [
                "Size",
                "Color"
            ]
        },
        {
            "objectTypeId": "nx.base.Bird",
            "attributes":
            [
                "Size",
                "Color"
            ]
        }
    ]
}
)json";

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
