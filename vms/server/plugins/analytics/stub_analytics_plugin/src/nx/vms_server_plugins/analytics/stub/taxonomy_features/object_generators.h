// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/ptr.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace taxonomy_features {

nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata> generateInstanceOfBaseObjectType();
nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata> generateInstanceOfDerivedObjectType();
nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata>
    generateInstanceOfDerivedObjectTypeWithOmittedAttributes();
nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata> generateInstanceOfHiddenDerivedObjectType();
nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata>
    generateInstanceOfHiddenDerivedObjectTypeWithOwnAttributes();
nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata>
    generateInstanceOfDerivedObjectTypeWithUnsupportedBase();
nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata> generateInstanceOfObjectTypeWithNumericAttibutes();
nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata> generateInstanceOfObjectTypeWithBooleanAttibutes();
nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata> generateInstanceOfObjectTypeWithIcon();
nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata>
    generateInstanceOfObjectTypeInheritedFromBaseTypeLibraryType();
nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata>
    generateInstanceOfObjectTypeUsingBaseTypeLibraryEnumType();
nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata>
    generateInstanceOfObjectTypeUsingBaseTypeLibraryColorType();
nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata>
    generateInstanceOfObjectTypeUsingBaseTypeLibraryObjectType();
nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata> generateInstanceOfOfBaseTypeLibraryObjectType();
nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata>
    generateInstanceOfObjectTypeDeclaredInEngineManifest();
nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata> generateInstanceOfLiveOnlyObjectType();
nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata> generateInstanceOfNonIndexableObjectType();
nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata> generateInstanceOfExtendedObjectType();
nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata> generateInstanceOfObjectTypeWithAttributeList();

} // namespace taxonomy_features
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
