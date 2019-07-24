#pragma once

#include <QtCore/QMap>

#include <nx/vms/server/analytics/wrappers/types.h>

#include <nx/sdk/i_string_map.h>
#include <nx/sdk/analytics/i_metadata_types.h>

#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/helpers/string_map.h>

#include <nx/utils/range_adapters.h>

namespace nx::vms::server::analytics::wrappers {

sdk::Ptr<const sdk::analytics::IMetadataTypes> toSdkMetadataTypes(
    const MetadataTypes& metadataTypes);

template<typename T = QMap<QString, QString>>
sdk::Ptr<const sdk::IStringMap> toSdkStringMap(const T& stringMap)
{
    auto sdkStringMapPtr = sdk::makePtr<nx::sdk::StringMap>();
    for (const auto&[key, value] : nx::utils::constKeyValueRange(stringMap))
        sdkStringMapPtr->setItem(toString(key).toStdString(), toString(value).toStdString());

    return sdkStringMapPtr;
}

template<typename T = QMap<QString, QString>>
T fromSdkStringMap(sdk::Ptr<const sdk::IStringMap> sdkStringMap)
{
    if (!sdkStringMap)
        return {};

    if (sdkStringMap->count() <= 0)
        return {};

    T result;
    for (int i = 0; i < sdkStringMap->count(); ++i)
        result.insert(sdkStringMap->key(i), sdkStringMap->value(i));

    return result;
}

} // namespace nx::vms::server::analytics::wrappers
