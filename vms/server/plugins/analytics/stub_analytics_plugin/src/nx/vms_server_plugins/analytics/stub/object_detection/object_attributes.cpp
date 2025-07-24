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
            {"Gender", "Woman|Man|Non-binary"},
            {"Age", "Child|Adult|Senior"},
            {"Hat.Type", "Hijab|Cap|Helmet"},
            {"Color", "Red|Yellow|Blue|Green"},
            {"Brand", "Honda|Toyota|Ford|Mercedes"},
            {"Size", "Small|Medium|Large"},
            {"Type", "Sedan|SUV|Motorcycle|Truck"}
        }
    },
    {
        "nx.base.Face",
        {
            {"Gender", "Man|Woman|Non-binary"},
            {"Age", "Senior|Adult|Child"},
            {"Emotion", "Smile|Neutral|Angry|Surprised"},
            {"Hat.Type", "Cap|Beanie|None"},
            {"Hair Color", "Black|Brown|Blonde|Gray|Red"},
            {"Temperature", "36|37|38"},
            {"Name", "John Doe|Jane Smith|Alex Kim"},
            {"Mask", "false|true"},
            {"Glasses.Type", "Transparent|Sunglasses|None"},
            {"Helmet", "false|true"}
        }
    },
    {
        "nx.base.Unknown",
        {}
    },
    {
        "nx.base.LicensePlate",
        {
            {"Number", "ABC1234|XYZ9876|LMN4321"},
            {"Country", "USA|Canada|Mexico"},
            {"State/Province", "Texas|California|New York"},
            {"Size", "Regular|Small|Large"},
            {"Color", "White|Yellow|Blue"}
        }
    },
    {
        "nx.base.Vehicle",
        {
            {"Color", "Blue|Red|Black|White"},
            {"Speed", "65|70|80|90"},
            {"Brand", "Ford|Toyota|Honda|Chevrolet"},
            {"Model", "Fiesta|Corolla|Civic|Impala"},
            {"Size", "Small|Medium|Large"},
            {"License Plate.Number", "XYZ9876|ABC1234|LMN4321"},
            {"License Plate.Country", "USA|Canada|Mexico"},
            {"License Plate.State/Province", "Texas|California|Florida"},
            {"Driver buckled up", "true|false"},
            {"Lane", "1|2|3|4"}
        }
    },
    {
        "nx.base.Car",
        {
            {"Type", "Sedan|Coupe|Hatchback|SUV"},
            {"Color", "Yellow|Black|White|Silver"},
            {"Speed", "35|45|55|65"},
            {"Brand", "Kia|Hyundai|Nissan|Mazda"},
            {"Model", "Rio|Elantra|Sentra|Mazda3"},
            {"Size", "Medium|Small|Large"},
            {"License Plate", "true|false"},
            {"Driver buckled up", "false|true"},
            {"Lane", "1|2|3"}
        }
    },
    {
        "nx.base.Truck",
        {
            {"Type", "Medium Truck|Heavy Truck|Pickup"},
            {"Color", "Red|Blue|White|Gray"},
            {"Speed", "40|50|60|70"},
            {"Brand", "Dodge|Ford|Chevrolet|MAN"},
            {"Model", "RAM1500|F-150|Silverado|TGS"},
            {"Size", "Medium|Large"},
            {"License Plate", "false|true"},
            {"Driver buckled up", "false|true"},
            {"Lane", "1|2|3|4"}
        }
    },
    {
        "nx.base.Bus",
        {
            {"Type", "Microbus|Minibus|Coach"},
            {"Color", "Gray|White|Blue|Yellow"},
            {"Speed", "30|40|50"},
            {"Brand", "Mercedes|Volvo|Iveco|Setra"},
            {"Model", "Sprinter|Tourismo|Daily|TopClass"},
            {"Size", "Medium|Large"},
            {"License Plate", "true|false"},
            {"Driver buckled up", "true|false"},
            {"Lane", "2|3|4"}
        }
    },
    {
        "nx.base.Train",
        {
            {"Color", "Green|Blue|Gray|Red"},
            {"Speed", "20|40|60|80"},
            {"Brand", "Rigas Vagonbuves Rupnica|Siemens|Bombardier|Alstom"},
            {"Model", "ER2|Desiro|Regio|Coradia"},
            {"Size", "Large|Small"}
        }
    },
    {
        "nx.base.Tram",
        {
            {"Color", "Yellow|Red|White|Blue"},
            {"Speed", "25|35|45"},
            {"Brand", "Pesa|CAF|Alstom|Skoda"},
            {"Model", "Swing|Urbos|Citadis|ForCity"},
            {"Size", "Large|Small"}
        }
    },
    {
        "nx.base.Bike",
        {
            {"Type", "Motorcycle|Scooter|Bicycle"},
            {"Color", "Red|Black|Green|White"},
            {"Speed", "80|60|40|20"},
            {"Brand", "Honda|Yamaha|Kawasaki|Suzuki"},
            {"Model", "CB650R|R3|Ninja|GSX"},
            {"Size", "Medium|Small"},
            {"License Plate", "true|false"},
            {"Lane", "1|2|3"}
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
            {"Brand", "Cessna"},
            {"Model", "172"}
        }
    },
    {
        "nx.base.Animal",
        {
            {"Size", "Small|Medium|Large"},
            {"Color", "Gray|Brown|White|Black"}
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
