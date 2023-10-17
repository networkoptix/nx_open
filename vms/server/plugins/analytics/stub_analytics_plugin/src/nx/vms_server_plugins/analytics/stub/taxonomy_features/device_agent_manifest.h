// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace taxonomy_features {

static const std::string kDeviceAgentManifest = /*suppress newline*/ 1 + (const char*) R"json(
{
    "supportedTypes":
    [
        {
            "objectTypeId": "nx.base.Animal",
            "attributes":
            [
                "Color",
                "Size"
            ]
        },
        {
            "objectTypeId": "nx.stub.baseObjectType1",
            "attributes":
            [
                "(Base) String attribute",
                "(Base) Number attribute",
                "(Base) Boolean attribute",
                "(Base) Enum attribute",
                "(Base) Color attribute",
                "(Base) Object attribute.Nested Field 1",
                "(Base) Object attribute.Nested Field 2",
                "(Base) Show Conditional Attribute",
                "(Base) Conditional Attribute"
            ],
            "_comment": "Supported Attributes of a nested Object are declared via `.`."
        },
        {
            "objectTypeId": "nx.stub.derivedObjectType",
            "attributes":
            [
                "(Base) String attribute",
                "(Base) Number attribute",
                "(Base) Boolean attribute",
                "(Base) Enum attribute",
                "(Base) Color attribute",
                "(Base) Object attribute",
                "(Derived) attribute 1",
                "(Derived) attribute 2"
            ]
        },
        {
            "objectTypeId": "nx.stub.derivedObjectTypeWithOmittedAttributes",
            "attributes":
            [
                "(Base) String attribute",
                "(Derived) own attribute"
            ]
        },
        {
            "objectTypeId": "nx.stub.hiddenDerivedObjectType",
            "attributes":
            [
                "(Base) String attribute",
                "(Base) Number attribute"
            ]
        },
        {
            "objectTypeId": "nx.stub.hiddenDerivedObjectTypeWithOwnAttributes",
            "attributes":
            [
                "Own Attribute of a hidden derived Object Type"
            ]
        },
        {
            "objectTypeId": "nx.stub.derivedObjectTypeWithUnsupportedBase",
            "attributes":
            [
                "(Base 2) String attribute",
                "(Base 2) Number attribute",
                "(Base 2) Boolean attribute",
                "(Derived) Enum attribute"
            ]
        },
        {
            "objectTypeId": "nx.stub.objectTypeWithNumberAttributes",
            "attributes":
            [
                "Integer attribute",
                "Floating point attribute",
                "Number attribute with min value",
                "Number attribute with max value",
                "Number attribute with bounds",
                "Number Attribute with unit",
                "Number attribute (full example)"
            ]
        },
        {
            "objectTypeId": "nx.stub.objectTypeWithBooleanAttributes",
            "attributes":
            [
                "Boolean attribute 1",
                "Boolean attribute 2",
                "Boolean attribute 3",
                "Boolean attribute 4"
            ]
        },
        {
            "objectTypeId": "nx.stub.objectTypeWithIcon"
        },
        {
            "objectTypeId": "nx.stub.objectTypeInheritedFromBaseTypeLibraryType",
            "attributes":
            [
                "Gender",
                "Height",
                "Top Clothing Color",
                "Name",
                "Hat",
                "Custom Type string attribute"
            ]
        },
        {
            "objectTypeId": "nx.stub.objectTypeUsingBaseTypeLibraryEnumType",
            "attributes":
            [
                "Custom Type Enum attribute"
            ]
        },
        {
            "objectTypeId": "nx.stub.objectTypeUsingBaseTypeLibraryColorType",
            "attributes":
            [
                "Custom Type Color attribute"
            ]
        },
        {
            "objectTypeId": "nx.stub.objectTypeUsingBaseTypeLibraryObjectType",
            "attributes":
            [
                "Custom Type Object attribute",
                "Custom Type Object attribute.Size",
                "Custom Type Object attribute.Color",
                "Custom Type Object attribute.Type"
            ]
        },
        {
            "objectTypeId": "nx.stub.objectTypeFromEngineManifest",
            "attributes":
            [
                "String attribute (Engine)",
                "Enum attribute (Engine)",
                "Color attribute (Engine)",
                "Object attribute using Object Type from Base Type Library (Engine)"
            ]
        },
        {
            "objectTypeId": "nx.stub.liveOnlyObjectType",
            "attributes":
            [
                "Live-only Object Type attribute"
            ]
        },
        {
            "objectTypeId": "nx.stub.nonIndexableObjectType",
            "attributes":
            [
                "Non-indexable Object Type attribute"
            ]
        },
        {
            "objectTypeId": "nx.base.Vehicle",
            "attributes": [
                "Extended Vehicle attribute"
            ]
        },
        {
            "objectTypeId": "nx.stub.objectTypeWithAttributeList",
            "attributes": [
                "Attribute List attribute 1",
                "Attribute List attribute 2",
                "Regular attribute"
            ]
        }
    ],)json" + /* Workaround for long strings. */ std::string() + R"json(
    "typeLibrary":
    {
        "attributeLists": [
            {
                "id": "nx.stub.attributeList",
                "attributes": [
                    {
                        "type": "String",
                        "name": "Attribute List attribute 1"
                    },
                    {
                        "type": "String",
                        "name": "Attribute List attribute 2"
                    }
                ]
            }
        ],
        "extendedObjectTypes": [
            {
                "id": "nx.base.Vehicle",
                "attributes": [
                    {
                        "name": "Extended Vehicle attribute",
                        "type": "String"
                    }
                ]
            }
        ],
        "objectTypes":
        [
            {
                "id": "nx.stub.objectTypeWithAttributeList",
                "name": "Stub: Object Type with Attribute List",
                "attributes": [
                    {
                        "attributeList": "nx.stub.attributeList"
                    },
                    {
                        "name": "Regular attribute",
                        "type": "String"
                    }
                ]
            },
            {
                "_comment": "Base Object Type containing Attributes of all types. All Attributes are declared as supported in the \"supportedTypes\" list.",
                "id": "nx.stub.baseObjectType1",
                "name": "Stub: Base Object Type 1",
                "attributes":
                [
                    {
                        "type": "String",
                        "name": "(Base) String attribute"
                    },
                    {
                        "type": "Number",
                        "name": "(Base) Number attribute"
                    },
                    {
                        "type": "Boolean",
                        "name": "(Base) Boolean attribute"
                    },
                    {
                        "type": "Enum",
                        "subtype": "nx.stub.baseEnumType",
                        "name": "(Base) Enum attribute"
                    },
                    {
                        "type": "Color",
                        "subtype": "nx.stub.baseColorType",
                        "name": "(Base) Color attribute"
                    },
                    {
                        "type": "Object",
                        "subtype": "nx.stub.attributeObjectType",
                        "name": "(Base) Object attribute"
                    },
                    {
                        "name": "(Base) Show Conditional Attribute",
                        "type": "Boolean"
                    },
                    {
                        "name": "(Base) Conditional Attribute",
                        "type": "String",
                        "condition": "\"(Base) Show Conditional Attribute\" = true"
                    }
                ]
            },
            {
                "_comment": "Base Object Type that is not declared as supported in the \"supportedTypes\" list.",
                "id": "nx.stub.baseObjectType2",
                "name": "Stub: Base Object Type 2",
                "attributes":
                [
                    {
                        "type": "String",
                        "name": "(Base 2) String attribute"
                    },
                    {
                        "type": "Number",
                        "name": "(Base 2) Number attribute"
                    },
                    {
                        "type": "Boolean",
                        "name": "(Base 2) Boolean attribute"
                    }
                ]
            },
            {
                "_comment": "Simple derived Object Type containing some additional Attributes.",
                "id": "nx.stub.derivedObjectType",
                "name": "Stub: Derived Object Type",
                "base": "nx.stub.baseObjectType1",
                "attributes":
                [
                    {
                        "name": "(Derived) attribute 1",
                        "type": "String"
                    },
                    {
                        "name": "(Derived) attribute 2",
                        "type": "String"
                    }
                ]
            },
            {
                "_comment": "Derived Object Type that blacklists some Attributes of its base Type with \"omittedBaseAttributes\".",
                "id": "nx.stub.derivedObjectTypeWithOmittedAttributes",
                "name": "Stub: Derived Object Type with omitted attributes",
                "base": "nx.stub.baseObjectType1",
                "omittedBaseAttributes":
                [
                    "(Base) Number attribute",
                    "(Base) Boolean attribute",
                    "(Base) Enum attribute",
                    "(Base) Color attribute",
                    "(Base) Object attribute"
                ],
                "attributes":
                [
                    {
                        "name": "(Derived) own attribute",
                        "type": "Number"
                    }
                ]
            },
            {
                "_comment": "Hidden derived Object Type. Such Types are not shown in the Analytics Panel.",
                "id": "nx.stub.hiddenDerivedObjectType",
                "name": "Stub: Hidden derived object type",
                "base": "nx.stub.baseObjectType1",
                "flags": "hiddenDerivedType"
            },
            {
                "_comment": "Hidden derived Object Types can have their own Attributes. Such Attributes are shown when its base Type is selected in the Analytics Panel.",
                "id": "nx.stub.hiddenDerivedObjectTypeWithOwnAttributes",
                "name": "Stub: Hidden derived object type with own attributes",
                "base": "nx.stub.baseObjectType2",
                "flags": "hiddenDerivedType",
                "attributes":
                [
                    {
                        "type": "String",
                        "name": "Own Attribute of a hidden derived Object Type"
                    }
                ]
            },
            {
                "_comment": "Declaring a Derived Object Type makes its base supported as well.",
                "id": "nx.stub.derivedObjectTypeWithUnsupportedBase",
                "name": "Stub: Derived Object Type with unsupported base",
                "base": "nx.stub.baseObjectType2",
                "attributes":
                [
                    {
                        "type": "Enum",
                        "subtype": "nx.base.derivedEnumType",
                        "name": "Derived Object Type Enum attribute"
                    }
                ]
            },
            {
                "id": "nx.stub.objectTypeWithNumberAttributes",
                "name": "Stub: Object Type with Number attributes",
                "attributes":
                [
                    {
                        "type": "Number",
                        "subtype": "integer",
                        "name": "Integer attribute"
                    },
                    {
                        "type": "Number",
                        "subtype": "float",
                        "name": "Floating point attribute"
                    },
                    {
                        "type": "Number",
                        "name": "Number attribute with min value",
                        "minValue": 10
                    },
                    {
                        "type": "Number",
                        "name": "Number attribute with max value",
                        "maxValue": 100
                    },
                    {
                        "type": "Number",
                        "name": "Number attribute with bounds",
                        "minValue": -100,
                        "maxValue": 100
                    },
                    {
                        "type": "Number",
                        "name": "Number Attribute with unit",
                        "unit": "unit"
                    },
                    {
                        "type": "Number",
                        "subtype": "integer",
                        "name": "Number attribute (full example)",
                        "minValue": -100,
                        "maxValue": 100,
                        "unit": "other unit"
                    }
                ]
            },
            {
                "id": "nx.stub.objectTypeWithBooleanAttributes",
                "name": "Stub: Object Type with Boolean attributes",
                "attributes":
                [
                    {
                        "type": "Boolean",
                        "name": "Boolean attribute 1"
                    },
                    {
                        "type": "Boolean",
                        "name": "Boolean attribute 2"
                    },
                    {
                        "type": "Boolean",
                        "name": "Boolean attribute 3"
                    },
                    {
                        "type": "Boolean",
                        "name": "Boolean attribute 4"
                    }
                ]
            },
            {
                "_comment": "Object Types can be used as Attribute types, yielding a nested Object.",
                "id": "nx.stub.attributeObjectType",
                "name": "Stub: Attribute Object Type",
                "attributes":
                [
                    {
                        "type": "String",
                        "name": "Nested field 1"
                    },
                    {
                        "type": "Number",
                        "name": "Nested field 2"
                    }
                ]
            },
            {
                "id": "nx.stub.objectTypeWithIcon",
                "name": "Stub: Object Type with icon",
                "icon": "nx.base.car"
            },
            {
                "_comment": "Object Types can inherit from the Types declared in the Base Type Library.",
                "id": "nx.stub.objectTypeInheritedFromBaseTypeLibraryType",
                "name": "Stub: Object Type inherited from a Base Type Library Type",
                "base": "nx.base.Person",
                "omittedBaseAttributes":
                [
                    "Complexion",
                    "Age",
                    "Activity",
                    "Scarf",
                    "Body Shape",
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
                    "Temperature",
                    "Tattoo"
                ],
                "attributes":
                [
                    {
                        "type": "String",
                        "name": "Custom Type string attribute"
                    }
                ]
            },
            {
                "_comment": "Object Types can use Enum Types declared in the Base Type Library.",
                "id": "nx.stub.objectTypeUsingBaseTypeLibraryEnumType",
                "name": "Stub: Custom Type using Base Type Library Enum Type",
                "attributes":
                [
                    {
                        "type": "Enum",
                        "subtype": "nx.base.Length",
                        "name": "Custom Type Enum attribute"
                    }
                ]
            },
            {
                "_comment": "Object Types can use Color Types declared in the Base Type Library.",
                "id": "nx.stub.objectTypeUsingBaseTypeLibraryColorType",
                "name": "Stub: Custom Type using Base Type Library Color Type",
                "attributes":
                [
                    {
                        "type": "Color",
                        "subtype": "nx.base.Color",
                        "name": "Custom Type Color attribute"
                    }
                ]
            },
            {
                "_comment": "Base Type Library Object Types can be used to declare Attributes being nested Objects.",
                "id": "nx.stub.objectTypeUsingBaseTypeLibraryObjectType",
                "name": "Stub: Custom Type using Base Type Library Object Type",
                "attributes":
                [
                    {
                        "type": "Object",
                        "subtype": "nx.base.Bag",
                        "name": "Custom Type Object attribute"
                    }
                ]
            },
            {
                "_comment": "Objects of live-only types are not recorded to the database and archive and available in the Live mode only.",
                "id": "nx.stub.liveOnlyObjectType",
                "name": "Stub: Live-only Object Type",
                "flags": "liveOnly",
                "attributes":
                [
                    {
                        "type": "String",
                        "name": "Live-only Object Type attribute"
                    }
                ]
            },
            {
                "_comment": "Objects of non-indexable types aren't available for search, but still recorded to the archive.",
                "id": "nx.stub.nonIndexableObjectType",
                "name": "Stub: Non-indexable Object Type",
                "flags": "nonIndexable",
                "attributes":
                [
                    {
                        "type": "String",
                        "name": "Non-indexable Object Type attribute"
                    }
                ]
            }
        ],)json" + /* Workaround for long strings. */ std::string() + R"json(
        "enumTypes":
        [
            {
                "id": "nx.stub.baseEnumType",
                "name": "Stub: Base Enum Type",
                "items":
                [
                    "Base Enum Type item 1",
                    "Base Enum Type item 2",
                    "Base Enum Type item 3"
                ]
            },
            {
                "id": "nx.stub.derivedEnumType",
                "name": "Stub: Derived Enum Type",
                "base": "nx.stub.baseEnumType",
                "baseItems":
                [
                    "Base Enum Type item 1",
                    "Base Enum Type item 2"
                ],
                "items":
                [
                    "Derived Enum Type item 1",
                    "Derived Enum Type item 2"
                ]
            }
        ],
        "colorTypes":
        [
            {
                "id": "nx.stub.baseColorType",
                "name": "Stub: Base Color Type",
                "items":
                [
                    {
                        "name": "black",
                        "rgb": "#000000"
                    },
                    {
                        "name": "grey",
                        "rgb": "#808080"
                    },
                    {
                        "name": "white",
                        "rgb": "#FFFFFF"
                    }
                ]
            },
            {
                "id": "nx.stub.derivedColorType",
                "name": "Stub: Derived Color Type",
                "base": "nx.stub.baseColorType",
                "baseItems":
                [
                    "black",
                    "white"
                ],
                "items":
                [
                    {
                        "name": "red",
                        "rgb": "#FF0000"
                    },
                    {
                        "name": "green",
                        "rgb": "#00FF00"
                    },
                    {
                        "name": "blue",
                        "rgb": "#0000FF"
                    }
                ]
            }
        ]
    }
}
)json";

} // namespace taxonomy_features
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
