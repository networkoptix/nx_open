// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "thumbnail_request_data.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/utils/string.h>
#include <nx/utils/datetime.h>
#include <nx/network/rest/params.h>

#include <api/helpers/camera_id_helper.h>

#include <nx/fusion/model_functions.h>
#include <nx/reflect/string_conversion.h>

namespace {

static const QString kDeprecatedPhysicalIdParam = "physicalId";
static const QString kDeprecatedMacParam = "mac";
static const QString kCameraIdParam = "cameraId";
static const QString kTimeParam = "time";
static const QString kIgnoreExternalArchiveParam = "ignoreExternalArchive";
static const QString kRotateParam = "rotate";
static const QString kCropParam = "crop";
static const QString kHeightParam = "height";
static const QString kDeprecatedWidthParam = "widht";
static const QString kWidthParam = "width";
static const QString kImageFormatParam = "imageFormat";
static const QString kRoundMethodParam = "method";
static const QString kAspectRatioParam = "aspectRatio";
static const QString kStreamSelectionModeParam = "streamSelectionMode";
static const QString kTolerantParam = "tolerant";
static const QString kObjectTrackIdParam = "objectTrackId";

static const QString kLatestTimeValue = "latest";

} // namespace

void QnThumbnailRequestData::loadFromParams(
    QnResourcePool* resourcePool, const nx::network::rest::Params& params)
{
    QnMultiserverRequestData::loadFromParams(resourcePool, params);

    request.camera = nx::camera_id_helper::findCameraByFlexibleIds(
        resourcePool,
        /*outNotFoundCameraId*/ nullptr,
        params,
        {kCameraIdParam, kDeprecatedPhysicalIdParam, kDeprecatedMacParam})
        .dynamicCast<QnVirtualCameraResource>();

    if (params.contains(kTimeParam))
    {
        QString timeValue = params.value(kTimeParam);
        if (timeValue.toLower() == kLatestTimeValue)
            request.timestampMs = nx::api::ImageRequest::kLatestThumbnail;
        else
            request.timestampMs = std::chrono::milliseconds(nx::utils::parseDateTimeMsec(timeValue));

        if (params.contains(kIgnoreExternalArchiveParam))
            request.ignoreExternalArchive = true;

        if (params.contains(kTolerantParam))
            request.tolerant = true;
    }

    request.rotation = QnLexical::deserialized(params.value(kRotateParam), request.rotation);
    request.crop = QnLexical::deserialized<QRectF>(params.value(kCropParam), request.crop);
    QSize& size = request.size;
    size.setHeight(QnLexical::deserialized(params.value(kHeightParam), size.height()));
    if (params.contains(kWidthParam))
        size.setWidth(QnLexical::deserialized(params.value(kWidthParam), size.width()));
    else
        size.setWidth(QnLexical::deserialized(params.value(kDeprecatedWidthParam), size.width()));
    request.format = nx::reflect::fromString(
        params.value(kImageFormatParam).toStdString(), /*defaultValue*/ request.format);
    request.roundMethod = nx::reflect::fromString(
        params.value(kRoundMethodParam).toStdString(), /*defaultValue*/ request.roundMethod);
    request.aspectRatio = nx::reflect::fromString(
        params.value(kAspectRatioParam).toStdString(), /*defaultValue*/ request.aspectRatio);
    request.streamSelectionMode = nx::reflect::fromString(
        params.value(kStreamSelectionModeParam).toStdString(),
        /*defaultValue*/ request.streamSelectionMode);

    // NOTE: Currently, there is no way to return an error from this function, thus, we use a safe
    // conversion here, and rely on the caller to properly handle the null value.
    request.objectTrackId = nx::Uuid::fromStringSafe(params.value(kObjectTrackIdParam));
}

nx::network::rest::Params QnThumbnailRequestData::toParams() const
{
    auto result = QnMultiserverRequestData::toParams();

    result.insert(kCameraIdParam, request.camera ? request.camera->getId().toString() : QString());
    result.insert(kTimeParam,
        nx::api::CameraImageRequest::isSpecialTimeValue(request.timestampMs)
            ? kLatestTimeValue
            : QString::number(request.timestampMs.count()));
    if (request.ignoreExternalArchive)
        result.insert(kIgnoreExternalArchiveParam, "");
    if (request.tolerant)
        result.insert(kTolerantParam, "");
    result.insert(kRotateParam, request.rotation);
    if (!request.crop.isNull())
        result.insert(kCropParam, QnLexical::serialized(request.crop));
    result.insert(kHeightParam, request.size.size.height());
    result.insert(kWidthParam, request.size.size.width());
    result.insert(kImageFormatParam, nx::reflect::toString(request.format));
    result.insert(kRoundMethodParam, nx::reflect::toString(request.roundMethod));
    result.insert(kAspectRatioParam, nx::reflect::toString(request.aspectRatio));
    result.insert(kStreamSelectionModeParam, nx::reflect::toString(request.streamSelectionMode));
    result.insert(kObjectTrackIdParam, request.objectTrackId.toByteArray());
    return result;
}

std::optional<QString> QnThumbnailRequestData::getError() const
{
    if (!request.camera)
        return lit("No camera available");

    // Check invalid time.
    if (request.timestampMs.count() < 0
        && request.timestampMs != nx::api::ImageRequest::kLatestThumbnail)
    {
        return lit("Invalid time");
    }
    if (request.ignoreExternalArchive
        && request.timestampMs != nx::api::ImageRequest::kLatestThumbnail
        && request.timestampMs.count() != DATETIME_NOW)
    {
        return QString("'%1' applies only for \"latest\" timestamp")
            .arg(kIgnoreExternalArchiveParam);
    }

    const QSize size = request.size;
    if (size.height() > 0 && size.height() < nx::api::CameraImageRequest::kMinimumSize)
        return QString("Height cannot be less than %1").arg(nx::api::CameraImageRequest::kMinimumSize);
    if (size.width() > 0 && size.width() < nx::api::CameraImageRequest::kMinimumSize)
        return QString("Width cannot be less than %1").arg(nx::api::CameraImageRequest::kMinimumSize);
    if (size.width() > 0 && size.height() < 0)
        return lit("Width cannot be specified without specifying height");

    return std::nullopt;
}
