#include "conversion_utils.h"

#include <QtCore/QJsonDocument>

#include <nx/sdk/i_settings_response.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/i_string_map.h>

#include <nx/sdk/analytics/helpers/pixel_format.h>
#include <nx/sdk/analytics/helpers/metadata_types.h>

#include <nx/utils/log/assert.h>

namespace nx::vms::server::sdk_support {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using IMetadataTypes = nx::sdk::analytics::IMetadataTypes;

Ptr<const IString> toSdkString(const QString& str)
{
    if (str.isEmpty())
        return nullptr;

    return makePtr<String>(str.toStdString().c_str());
}

Ptr<const IString> toSdkString(const std::string& str)
{
    if (str.empty())
        return nullptr;

    return makePtr<String>(str.c_str());
}

Ptr<const IMetadataTypes> toSdkMetadataTypes(const sdk_support::MetadataTypes& metadataTypes)
{
    auto sdkMetadataTypesPtr = makePtr<sdk::analytics::MetadataTypes>();
    for (const auto& eventTypeId: metadataTypes.eventTypeIds)
        sdkMetadataTypesPtr->addEventTypeId(eventTypeId.toStdString());

    for (const auto& objectTypeId: metadataTypes.objectTypeIds)
        sdkMetadataTypesPtr->addObjectTypeId(objectTypeId.toStdString());

    return sdkMetadataTypesPtr;
}

MetadataTypes fromSdkMetadataTypes(const Ptr<const IMetadataTypes>& sdkMetadataTypes)
{
    sdk_support::MetadataTypes metadataTypes;
    metadataTypes.eventTypeIds = fromSdkStringList<std::set<QString>>(
        sdkMetadataTypes->eventTypeIds());

    metadataTypes.objectTypeIds = fromSdkStringList<std::set<QString>>(
        sdkMetadataTypes->objectTypeIds());

    return metadataTypes;
}

//-------------------------------------------------------------------------------------------------

nx::vms::api::analytics::PixelFormat fromSdkPixelFormat(
    analytics::IUncompressedVideoFrame::PixelFormat sdkPixelFormat)
{
    using namespace nx::vms::api::analytics;

    switch (sdkPixelFormat)
    {
        case IUncompressedVideoFrame::PixelFormat::yuv420:
            return PixelFormat::yuv420;
        case IUncompressedVideoFrame::PixelFormat::argb:
            return PixelFormat::argb;
        case IUncompressedVideoFrame::PixelFormat::abgr:
            return PixelFormat::abgr;
        case IUncompressedVideoFrame::PixelFormat::rgba:
            return PixelFormat::rgba;
        case IUncompressedVideoFrame::PixelFormat::bgra:
            return PixelFormat::bgra;
        case IUncompressedVideoFrame::PixelFormat::rgb:
            return PixelFormat::rgb;
        case IUncompressedVideoFrame::PixelFormat::bgr:
            return PixelFormat::bgr;
        default:
            NX_ASSERT(false, lm("Wrong pixel format, %1").args((int)sdkPixelFormat));
            return PixelFormat::undefined;
    }
}

std::optional<IUncompressedVideoFrame::PixelFormat> toSdkPixelFormat(
    nx::vms::api::analytics::PixelFormat pixelFormat)
{
    using namespace nx::vms::api::analytics;

    switch (pixelFormat)
    {
        case PixelFormat::yuv420:
            return IUncompressedVideoFrame::PixelFormat::yuv420;
        case PixelFormat::argb:
            return IUncompressedVideoFrame::PixelFormat::argb;
        case PixelFormat::abgr:
            return IUncompressedVideoFrame::PixelFormat::abgr;
        case PixelFormat::rgba:
            return IUncompressedVideoFrame::PixelFormat::rgba;
        case PixelFormat::bgra:
            return IUncompressedVideoFrame::PixelFormat::bgra;
        case PixelFormat::rgb:
            return IUncompressedVideoFrame::PixelFormat::rgb;
        case PixelFormat::bgr:
            return IUncompressedVideoFrame::PixelFormat::bgr;
        default:
            NX_ASSERT(false, lm("Wrong pixel format").args((int)pixelFormat));
            return std::nullopt;
    }
}

AVPixelFormat sdkToAvPixelFormat(IUncompressedVideoFrame::PixelFormat pixelFormat)
{
    using PixelFormat = IUncompressedVideoFrame::PixelFormat;
    switch (pixelFormat)
    {
        case PixelFormat::yuv420: return AV_PIX_FMT_YUV420P;
        case PixelFormat::argb: return AV_PIX_FMT_ARGB;
        case PixelFormat::abgr: return AV_PIX_FMT_ABGR;
        case PixelFormat::rgba: return AV_PIX_FMT_RGBA;
        case PixelFormat::bgra: return AV_PIX_FMT_BGRA;
        case PixelFormat::rgb: return AV_PIX_FMT_RGB24;
        case PixelFormat::bgr: return AV_PIX_FMT_BGR24;

        default:
            NX_ASSERT(false, lm("Unsupported PixelFormat value: %1").arg(
                pixelFormatToStdString(pixelFormat)));
            return AV_PIX_FMT_NONE;
    }
}

AVPixelFormat apiToAvPixelFormat(nx::vms::api::analytics::PixelFormat pixelFormat)
{
    using PixelFormat = nx::vms::api::analytics::PixelFormat;
    switch (pixelFormat)
    {
        case PixelFormat::yuv420: return AV_PIX_FMT_YUV420P;
        case PixelFormat::argb: return AV_PIX_FMT_ARGB;
        case PixelFormat::abgr: return AV_PIX_FMT_ABGR;
        case PixelFormat::rgba: return AV_PIX_FMT_RGBA;
        case PixelFormat::bgra: return AV_PIX_FMT_BGRA;
        case PixelFormat::rgb: return AV_PIX_FMT_RGB24;
        case PixelFormat::bgr: return AV_PIX_FMT_BGR24;

        default:
            NX_ASSERT(false, "Unsupported PixelFormat value: %1", (int) pixelFormat);
            return AV_PIX_FMT_NONE;
    }
}

std::optional<nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat> avPixelFormatToSdk(
    AVPixelFormat avPixelFormat)
{
    using PixelFormat = IUncompressedVideoFrame::PixelFormat;
    switch (avPixelFormat)
    {
        case AV_PIX_FMT_YUV420P: return PixelFormat::yuv420;
        case AV_PIX_FMT_ARGB: return PixelFormat::argb;
        case AV_PIX_FMT_ABGR: return PixelFormat::abgr;
        case AV_PIX_FMT_RGBA: return PixelFormat::rgba;
        case AV_PIX_FMT_BGRA: return PixelFormat::bgra;
        case AV_PIX_FMT_RGB24: return PixelFormat::rgb;
        case AV_PIX_FMT_BGR24: return PixelFormat::bgr;
        default: return std::nullopt;
    }
}

SdkSettingsResponse toSdkSettingsResponse(
    const ResultHolder<const sdk::ISettingsResponse*>& settingsResponse)
{
    SdkSettingsResponse result;

    result.sdkError = Error::fromResultHolder(settingsResponse);

    const sdk::Ptr<const IRefCountable> value = settingsResponse.value();
    if (!value)
        return result;

    if (const auto settingsResponse = value->queryInterface<nx::sdk::ISettingsResponse>())
    {
        result.values = SettingsValues();
        if (const sdk::Ptr<const IStringMap> settingsValues = settingsResponse->values())
            result.values = fromSdkStringMap<SettingsValues>(settingsValues);

        if (const sdk::Ptr<const IStringMap> settingsErrors = settingsResponse->errors())
            result.errors = fromSdkStringMap<SettingsErrors>(settingsErrors);

        if (const sdk::Ptr<const IString> settingsModel = settingsResponse->model())
        {
            const auto modelString = fromSdkString<QString>(settingsModel);
            QJsonDocument doc = QJsonDocument::fromJson(modelString.toUtf8());
            if (doc.isObject())
                result.model = doc.object();
        }
    }
    else if (const auto settingsResponse0 = value->queryInterface<nx::sdk::ISettingsResponse0>())
    {
        // This type of response may be returned by pluginSideSettings() of 4.0 plugins.

        result.values = SettingsValues();
        if (const sdk::Ptr<const IStringMap> settingsValues = settingsResponse0->values())
            result.values = fromSdkStringMap<sdk_support::SettingsValues>(settingsValues);

        if (const sdk::Ptr<const IStringMap> settingsErrors = settingsResponse0->errors())
            result.errors = fromSdkStringMap<sdk_support::SettingsErrors>(settingsErrors);
    }
    else if (const auto stringMap = value->queryInterface<nx::sdk::IStringMap>())
    {
        // This type of response may be returned by setSettings() of 4.0 plugins.
        result.errors = fromSdkStringMap<sdk_support::SettingsErrors>(stringMap);
    }

    return result;
}


} // namespace nx::vms::server::sdk_support
