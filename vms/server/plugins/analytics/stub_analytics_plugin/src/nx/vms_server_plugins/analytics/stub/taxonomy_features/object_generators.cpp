// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_generators.h"

#include <nx/sdk/helpers/attribute.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace taxonomy_features {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Ptr<ObjectMetadata> generateInstanceOfBaseObjectType()
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId("nx.stub.baseObjectType1");
    objectMetadata->addAttributes({
        makePtr<Attribute>(
            "(Base) String attribute",
            "Base String attribute value"),
        makePtr<Attribute>(
            "(Base) Number attribute",
            "1"),
        makePtr<Attribute>(
            "(Base) Boolean attribute",
            "true"),
        makePtr<Attribute>(
            "(Base) Enum attribute",
            "Base Enum Type item 1"),
        makePtr<Attribute>(
            "(Base) Color attribute",
            "Base String attribute value"),
        makePtr<Attribute>(
            "(Base) Object attribute.Nested Field 1",
            "Nested Object attribute value"),
        makePtr<Attribute>(
            "(Base) Object attribute.Nested Field 2",
            "2.5"),
        makePtr<Attribute>(
            "(Base) Show Conditional Attribute",
            "true"),
        makePtr<Attribute>(
            "(Base) Conditional Attribute",
            "String value")
    });

    return objectMetadata;
}

Ptr<ObjectMetadata> generateInstanceOfDerivedObjectType()
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId("nx.stub.derivedObjectType");
    objectMetadata->addAttributes({
        makePtr<Attribute>(
            "(Base) String attribute",
            "Base String attribute value"),
        makePtr<Attribute>(
            "(Base) Number attribute",
            "1"),
        makePtr<Attribute>(
            "(Base) Boolean attribute",
            "true"),
        makePtr<Attribute>(
            "(Base) Enum attribute",
            "Base Enum Type item 1"),
        makePtr<Attribute>(
            "(Base) Color attribute",
            "black"),
        makePtr<Attribute>(
            "(Base) Object attribute",
            "true"),
        makePtr<Attribute>(
            "(Derived) attribute 1",
            "Derived String attribute 1 value"),
        makePtr<Attribute>(
            "(Derived) attribute 2",
            "Derived String attribute 2 value"),
    });

    return objectMetadata;
}

Ptr<ObjectMetadata> generateInstanceOfDerivedObjectTypeWithOmittedAttributes()
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId("nx.stub.derivedObjectTypeWithOmittedAttributes");
    objectMetadata->addAttributes({
        makePtr<Attribute>(
            "(Base) String attribute",
            "Base String attribute value"),
        makePtr<Attribute>(
            "(Derived) own attribute",
            "5"),
    });

    return objectMetadata;
}

Ptr<ObjectMetadata> generateInstanceOfHiddenDerivedObjectType()
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId("nx.stub.hiddenDerivedObjectType");
    objectMetadata->addAttributes({
        makePtr<Attribute>(
            "(Base) String attribute",
            "Base String attribute value (Hidden derived Object Type)"),
        makePtr<Attribute>(
            "(Base) Number attribute",
            "75.2"),
    });

    return objectMetadata;
}

Ptr<ObjectMetadata> generateInstanceOfHiddenDerivedObjectTypeWithOwnAttributes()
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId("nx.stub.hiddenDerivedObjectTypeWithOwnAttributes");
    objectMetadata->addAttributes({
        makePtr<Attribute>(
            "Own Attribute of a hidden derived Object Type",
            "Attribute value"),
    });

    return objectMetadata;
}

Ptr<ObjectMetadata> generateInstanceOfDerivedObjectTypeWithUnsupportedBase()
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId("nx.stub.derivedObjectTypeWithUnsupportedBase");
    objectMetadata->addAttributes({
        makePtr<Attribute>(
            "(Base 2) String attribute",
            "String attribute value"),
        makePtr<Attribute>(
            "(Base 2) Number attribute",
            "2"),
        makePtr<Attribute>(
            "(Base 2) Boolean attribute",
            "1"),
        makePtr<Attribute>(
            "(Derived) Enum attribute",
            "Derived Enum Type item 2"),
    });

    return objectMetadata;
}

Ptr<ObjectMetadata> generateInstanceOfObjectTypeWithNumericAttibutes()
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId("nx.stub.objectTypeWithNumberAttributes");
    objectMetadata->addAttributes({
        makePtr<Attribute>(
            "Integer attribute",
            "21"),
        makePtr<Attribute>(
            "Floating point attribute",
            "2.5"),
        makePtr<Attribute>(
            "Number attribute with min value",
            "15"),
        makePtr<Attribute>(
            "Number attribute with max value",
            "99"),
        makePtr<Attribute>(
            "Number attribute with bounds",
            "-99"),
        makePtr<Attribute>(
            "Number Attribute with unit",
            "26"),
        makePtr<Attribute>(
            "Number attribute (full example)",
            "-12.5"),
    });

    return objectMetadata;
}

Ptr<ObjectMetadata> generateInstanceOfObjectTypeWithBooleanAttibutes()
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId("nx.stub.objectTypeWithBooleanAttributes");
    objectMetadata->addAttributes({
        makePtr<Attribute>(
            "Boolean attribute 1",
            "1"),
        makePtr<Attribute>(
            "Boolean attribute 2",
            "0"),
        makePtr<Attribute>(
            "Boolean attribute 3",
            "true"),
        makePtr<Attribute>(
            "Boolean attribute 4",
            "false"),
    });

    return objectMetadata;
}

Ptr<ObjectMetadata> generateInstanceOfObjectTypeWithIcon()
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId("nx.stub.objectTypeWithIcon");
    return objectMetadata;
}

Ptr<ObjectMetadata> generateInstanceOfObjectTypeInheritedFromBaseTypeLibraryType()
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId("nx.stub.objectTypeInheritedFromBaseTypeLibraryType");
    objectMetadata->addAttributes({
        makePtr<Attribute>(
            "Gender",
            "Man"),
        makePtr<Attribute>(
            "Height",
            "180"),
        makePtr<Attribute>(
            "Top Clothing Color",
            "yellow"),
        makePtr<Attribute>(
            "Name",
            "John Doe"),
        makePtr<Attribute>(
            "Hat",
            "true"),
        makePtr<Attribute>(
            "Custom Type string attribute",
            "Person description"),
    });

    return objectMetadata;
}

Ptr<ObjectMetadata> generateInstanceOfObjectTypeUsingBaseTypeLibraryEnumType()
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId("nx.stub.objectTypeUsingBaseTypeLibraryEnumType");
    objectMetadata->addAttributes({
        makePtr<Attribute>(
            "Custom Type Enum attribute",
            "Short"),
    });

    return objectMetadata;
}

Ptr<ObjectMetadata> generateInstanceOfObjectTypeUsingBaseTypeLibraryColorType()
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId("nx.stub.objectTypeUsingBaseTypeLibraryColorType");
    objectMetadata->addAttributes({
        makePtr<Attribute>(
            "Custom Type Color attribute",
            "green"),
    });

    return objectMetadata;
}

Ptr<ObjectMetadata> generateInstanceOfObjectTypeUsingBaseTypeLibraryObjectType()
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId("nx.stub.objectTypeUsingBaseTypeLibraryObjectType");
    objectMetadata->addAttributes({
        makePtr<Attribute>(
            "Custom Type Object attribute.Size",
            "Small"),
        makePtr<Attribute>(
            "Custom Type Object attribute.Color",
            "red"),
        makePtr<Attribute>(
            "Custom Type Object attribute.Type",
            "Backpack"),
    });

    return objectMetadata;
}

Ptr<ObjectMetadata> generateInstanceOfOfBaseTypeLibraryObjectType()
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId("nx.base.Animal");
    objectMetadata->addAttributes({
        makePtr<Attribute>(
            "Size",
            "Medium"),
        makePtr<Attribute>(
            "Color",
            "gray"),
    });

    return objectMetadata;
}

Ptr<ObjectMetadata> generateInstanceOfObjectTypeDeclaredInEngineManifest()
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId("nx.stub.objectTypeFromEngineManifest");
    objectMetadata->addAttributes({
        makePtr<Attribute>(
            "String attribute (Engine)",
            "String attribute value (Engine)"),
        makePtr<Attribute>(
            "Enum attribute (Engine)",
            "Engine item 1"),
        makePtr<Attribute>(
            "Color attribute (Engine)",
            "blue"),
        makePtr<Attribute>(
            "Object attribute using Object Type from Base Type Library (Engine)",
            "true"),
    });

    return objectMetadata;
}

Ptr<ObjectMetadata> generateInstanceOfLiveOnlyObjectType()
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId("nx.stub.liveOnlyObjectType");
    objectMetadata->addAttributes({
        makePtr<Attribute>(
            "Live-only Object Type attribute",
            "Some value"),
        });

    return objectMetadata;
}

Ptr<ObjectMetadata> generateInstanceOfNonIndexableObjectType()
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId("nx.stub.nonIndexableObjectType");
    objectMetadata->addAttributes({
        makePtr<Attribute>(
            "Non-indexable Object Type attribute",
            "Some value"),
        });

    return objectMetadata;
}

Ptr<ObjectMetadata> generateInstanceOfExtendedObjectType()
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId("nx.stub.taxonomy_features$nx.base.Vehicle");
    objectMetadata->addAttributes({
        makePtr<Attribute>(
            "Extended Vehicle attribute",
            "Some value"),
        });

    return objectMetadata;
}

Ptr<ObjectMetadata> generateInstanceOfObjectTypeWithAttributeList()
{
    auto objectMetadata = makePtr<ObjectMetadata>();
    objectMetadata->setTypeId("nx.stub.objectTypeWithAttributeList");
    objectMetadata->addAttributes({
        makePtr<Attribute>(
            "Attribute List attribute 1",
            "Some value 1"),
        makePtr<Attribute>(
            "Attribute List attribute 2",
            "Some value 2"),
        makePtr<Attribute>(
            "Regular attribute",
            "Some value 3")
        });

    return objectMetadata;
}

} // namespace taxonomy_features
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
