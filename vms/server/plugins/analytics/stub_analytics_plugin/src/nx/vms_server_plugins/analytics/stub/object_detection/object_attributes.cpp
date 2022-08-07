// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_attributes.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_detection {

const std::map<std::string, std::map<std::string, std::string>> kObjectAttributes = {
    {
        "nx.base.Person",
        {
            {"Gender", "Woman"},
            {"Age", "Adult"},
            {"Hair Color", "Red"},
            {"Hat.Type", "Hijab"},
            {"Name", "Jane Doe"},
            {"Bag.Color", "Yellow"},
            {"Weapon", "false"},
            {"Mask", "true"},
            {"Glasses.Type", "Sunglasses"},
            {"Helmet", "false"}
        }
    },
    {
        "nx.base.Face",
        {
            {"Gender", "Man"},
            {"Age", "Senior"},
            {"Emotion", "Smile"},
            {"Hat.Type", "Cap"},
            {"Hair Color", "Black"},
            {"Temperature", "36"},
            {"Name", "John Doe"},
            {"Mask", "false"},
            {"Glasses.Type", "Transparent"},
            {"Helmet", "false"}
        }
    },
    {
        "nx.base.Unknown",
        {}
    },
    {
        "nx.base.LicensePlate",
        {
            {"Number", "ABC1234"},
            {"Country", "USA"},
            {"State/Province", "Texas"},
            {"Size", "Regular"},
            {"Color", "White"}
        }
    },
    {
        "nx.base.Vehicle",
        {
            {"Color", "Blue"},
            {"Speed", "65"},
            {"Brand", "Ford"},
            {"Model", "Fiesta"},
            {"Size", "Small"},
            {"License Plate.Number", "XYZ9876"},
            {"License Plate.Country", "USA"},
            {"License Plate.State/Province", "Texas"},
            {"Driver buckled up", "true"},
            {"Lane", "2"}
        }
    },
    {
        "nx.base.Car",
        {
            {"Type", "Sedan"},
            {"Color", "Yellow"},
            {"Speed", "35"},
            {"Brand", "Kia"},
            {"Model", "Rio"},
            {"Size", "Medium"},
            {"License Plate", "true"},
            {"Driver buckled up", "false"},
            {"Lane", "1"}
        }
    },
    {
        "nx.base.Truck",
        {
            {"Type", "Medium Truck"},
            {"Color", "Red"},
            {"Speed", "40"},
            {"Brand", "Dodge"},
            {"Model", "RAM1500"},
            {"Size", "Medium"},
            {"License Plate", "false"},
            {"Driver buckled up", "false"},
            {"Lane", "3"}
        }
    },
    {
        "nx.base.Bus",
        {
            {"Type", "Microbus"},
            {"Color", "Gray"},
            {"Speed", "30"},
            {"Brand", "Mercedes"},
            {"Model", "Sprinter"},
            {"Size", "Medium"},
            {"License Plate", "true"},
            {"Driver buckled up", "true"},
            {"Lane", "4"}
        }
    },
    {
        "nx.base.Train",
        {
            {"Color", "Green"},
            {"Speed", "20"},
            {"Brand", "Rigas Vagonbuves Rupnica"},
            {"Model", "ER2"},
            {"Size", "Large"}
        }
    },
    {
        "nx.base.Tram",
        {
            {"Color", "Yellow"},
            {"Speed", "25"},
            {"Brand", "Pesa"},
            {"Model", "Swing"},
            {"Size", "Large"}
        }
    },
    {
        "nx.base.Bike",
        {
            {"Type", "Motorcycle"},
            {"Color", "Red"},
            {"Speed", "80"},
            {"Brand", "Honda"},
            {"Model", "CB650R"},
            {"Size", "Medium"},
            {"License Plate", "true"},
            {"Lane", "1"}
        }
    },
    {
        "nx.base.Special",
        {
            {"Type", "Ambulance"},
            {"Color", "White"},
            {"Speed", "70"},
            {"License Plate", "true"},
            {"Driver buckled up", "true"},
            {"Lane", "2"}
        }
    },
    {
        "nx.base.WaterTransport",
        {
            {"Color", "Gray"},
            {"Speed", "20"},
            {"Size", "Small"}
        }
    },
    {
        "nx.base.AirTransport",
        {
            {"Color", "Orange"},
            {"Speed", "250"},
            {"Brand", "Cesna"},
            {"Model", "172"}
        }
    },
    {
        "nx.base.Animal",
        {
            {"Size", "Small"},
            {"Color", "Gray"}
        }
    },
    {
        "nx.base.Cat",
        {
            {"Size", "Large"},
            {"Color", "Yellow"}
        }
    },
    {
        "nx.base.Dog",
        {
            {"Size", "Medium"},
            {"Color", "Brown"}
        }
    },
    {
        "nx.base.Fish",
        {
            {"Size", "Small"},
            {"Color", "Orange"}
        }
    },
    {
        "nx.base.Snake",
        {
            {"Size", "Medium"},
            {"Color", "Green"}
        }
    },
    {
        "nx.base.Bird",
        {
            {"Size", "Small"},
            {"Color", "Red"}
        }
    }
};

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
