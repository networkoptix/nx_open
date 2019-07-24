#include "helpers.h"

#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/analytics/helpers/metadata_types.h>

namespace nx::vms::server::analytics::wrappers {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Ptr<const IMetadataTypes> toSdkMetadataTypes(const MetadataTypes& metadataTypes)
{
    auto sdkMetadataTypesPtr = makePtr<nx::sdk::analytics::MetadataTypes>();
    for (const auto& eventTypeId: metadataTypes.eventTypeIds)
        sdkMetadataTypesPtr->addEventTypeId(eventTypeId.toStdString());

    for (const auto& objectTypeId: metadataTypes.objectTypeIds)
        sdkMetadataTypesPtr->addObjectTypeId(objectTypeId.toStdString());

    return sdkMetadataTypesPtr;
}

} // namespace nx::vms::server::analytics::wrappers
