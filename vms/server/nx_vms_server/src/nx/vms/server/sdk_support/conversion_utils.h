#pragma once

#include <type_traits>

#include <nx/vms/server/sdk_support/types.h>

#include <nx/sdk/i_string.h>
#include <nx/sdk/i_string_map.h>
#include <nx/sdk/i_string_list.h>
#include <nx/sdk/analytics/i_metadata_types.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>

#include <nx/sdk/ptr.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/string_list.h>
#include <nx/sdk/helpers/string_map.h>

#include <nx/vms/api/analytics/pixel_format.h>

#include <nx/utils/range_adapters.h>

namespace nx::vms::server::sdk_support {

// String utils -----------------------------------------------------------------------------------

template<typename T>
T fromSdkString(const sdk::Ptr<const sdk::IString> sdkString)
{
    if (!sdkString)
        return {};

    const char* const cString = sdkString->str();
    if (!cString)
        return {};

    return T{cString};
}

sdk::Ptr<const sdk::IString> toSdkString(const QString& str);

sdk::Ptr<const sdk::IString> toSdkString(const std::string& str);


// String list utils ------------------------------------------------------------------------------

template<typename Container>
Container fromSdkStringList(const sdk::Ptr<const sdk::IStringList> sdkStringList)
{
    if (!sdkStringList)
        return {};

    Container result;
    for (int i = 0; i < sdkStringList->count(); ++i)
    {
        const char* const cString = sdkStringList->at(i);
        if (!cString)
            continue;

        result.insert(result.end(), typename Container::value_type{cString});
    }

    return result;
}

template<typename Container>
const sdk::Ptr<const sdk::IStringList> toSdkStringList(const Container& stringList)
{
    const auto result = sdk::makePtr<sdk::StringList>();
    for (const auto& str: stringList)
        result->addString(toString(str).toStdString());

    return result;
}

// String map utils -------------------------------------------------------------------------------

template<typename Map>
sdk::Ptr<const sdk::IStringMap> toSdkStringMap(const Map& stringMap)
{
    auto sdkStringMapPtr = sdk::makePtr<nx::sdk::StringMap>();
    for (const auto&[key, value] : nx::utils::constKeyValueRange(stringMap))
        sdkStringMapPtr->setItem(toString(key).toStdString(), toString(value).toStdString());

    return sdkStringMapPtr;
}

template<typename Map>
Map fromSdkStringMap(sdk::Ptr<const sdk::IStringMap> sdkStringMap)
{
    if (!sdkStringMap)
        return {};

    if (sdkStringMap->count() <= 0)
        return {};

    Map result;
    for (int i = 0; i < sdkStringMap->count(); ++i)
        result.insert(sdkStringMap->key(i), sdkStringMap->value(i));

    return result;
}

// Metadata types utils ---------------------------------------------------------------------------

sdk::Ptr<const sdk::analytics::IMetadataTypes> toSdkMetadataTypes(
    const MetadataTypes& metadataTypes);

MetadataTypes fromSdkMetadataTypes(
    const sdk::Ptr<const sdk::analytics::IMetadataTypes>& sdkMetadataTypes);

// Pixel format utils -----------------------------------------------------------------------------

nx::vms::api::analytics::PixelFormat fromSdkPixelFormat(
    nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat sdkPixelFormat);

std::optional<nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat> toSdkPixelFormat(
    nx::vms::api::analytics::PixelFormat pixelFormat);

/** @return Converted value, or, on error, AV_PIX_FMT_NONE, after failing an assertion. */
AVPixelFormat sdkToAvPixelFormat(
    nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat sdkPixelFormat);

std::optional<nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat> avPixelFormatToSdk(
    AVPixelFormat avPixelFormat);

} // namespace nx::vms::server::sdk_support
