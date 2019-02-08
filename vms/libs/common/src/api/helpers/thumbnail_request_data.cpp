#include "thumbnail_request_data.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/utils/string.h>
#include <nx/utils/datetime.h>

#include <api/helpers/camera_id_helper.h>

#include <nx/fusion/model_functions.h>

namespace {

static const QString kDeprecatedPhysicalIdParam = "physicalId";
static const QString kDeprecatedMacParam = "mac";
static const QString kCameraIdParam = "cameraId";
static const QString kTimeParam = "time";
static const QString kIgnoreExternalArchive = "ignoreExternalArchive";
static const QString kRotateParam = "rotate";
static const QString kHeightParam = "height";
static const QString kDeprecatedWidthParam = "widht";
static const QString kWidthParam = "width";
static const QString kImageFormatParam = "imageFormat";
static const QString kRoundMethodParam = "method";
static const QString kAspectRatioParam = "aspectRatio";
static const QString kStreamSelectionModeParam = "streamSelectionMode";

static const QString kLatestTimeValue = "latest";

} // namespace

void QnThumbnailRequestData::loadFromParams(QnResourcePool* resourcePool,
    const QnRequestParamList& params)
{
    QnMultiserverRequestData::loadFromParams(resourcePool, params);

    request.camera = nx::camera_id_helper::findCameraByFlexibleIds(
        resourcePool,
        /*outNotFoundCameraId*/ nullptr,
        params.toHash(),
        {kCameraIdParam, kDeprecatedPhysicalIdParam, kDeprecatedMacParam})
        .dynamicCast<QnVirtualCameraResource>();

    if (params.contains(kTimeParam))
    {
        QString timeValue = params.value(kTimeParam);
        if (timeValue.toLower() == kLatestTimeValue)
            request.usecSinceEpoch = nx::api::ImageRequest::kLatestThumbnail;
        else
            request.usecSinceEpoch = nx::utils::parseDateTime(timeValue);

        if (params.contains(kIgnoreExternalArchive))
            request.ignoreExternalArchive = true;
    }

    request.rotation = QnLexical::deserialized<int>(params.value(kRotateParam), request.rotation);
    auto& size = request.size;
    size.setHeight(QnLexical::deserialized<int>(params.value(kHeightParam),
        size.height()));
    if (params.contains(kWidthParam))
        size.setWidth(QnLexical::deserialized<int>(params.value(kWidthParam), size.width()));
    else
        size.setWidth(QnLexical::deserialized<int>(params.value(kDeprecatedWidthParam), size.width()));
    request.imageFormat = QnLexical::deserialized<nx::api::ImageRequest::ThumbnailFormat>(
        params.value(kImageFormatParam), /*defaultValue*/ request.imageFormat);
    request.roundMethod = QnLexical::deserialized<nx::api::ImageRequest::RoundMethod>(
        params.value(kRoundMethodParam), /*defaultValue*/ request.roundMethod);
    request.aspectRatio = QnLexical::deserialized<nx::api::ImageRequest::AspectRatio>(
        params.value(kAspectRatioParam), /*defaultValue*/ request.aspectRatio);
    request.streamSelectionMode =
        QnLexical::deserialized<nx::api::CameraImageRequest::StreamSelectionMode>(
            params.value(kStreamSelectionModeParam), /*defaultValue*/ request.streamSelectionMode);
}

QnRequestParamList QnThumbnailRequestData::toParams() const
{
    QnRequestParamList result = QnMultiserverRequestData::toParams();

    result.insert(kCameraIdParam,
        QnLexical::serialized(request.camera ? request.camera->getId().toString() : QString()));
    result.insert(kTimeParam,
        nx::api::CameraImageRequest::isSpecialTimeValue(request.usecSinceEpoch)
        ? kLatestTimeValue
        : QnLexical::serialized(request.usecSinceEpoch));
    if (request.ignoreExternalArchive)
        result.insert(kIgnoreExternalArchive, "");
    result.insert(kRotateParam, QnLexical::serialized(request.rotation));
    result.insert(kHeightParam, QnLexical::serialized(request.size.height()));
    result.insert(kWidthParam, QnLexical::serialized(request.size.width()));
    result.insert(kImageFormatParam, QnLexical::serialized(request.imageFormat));
    result.insert(kRoundMethodParam, QnLexical::serialized(request.roundMethod));
    result.insert(kAspectRatioParam, QnLexical::serialized(request.aspectRatio));
    result.insert(kStreamSelectionModeParam, QnLexical::serialized(request.streamSelectionMode));
    return result;
}

boost::optional<QString> QnThumbnailRequestData::getError() const
{
    if (!request.camera)
        return lit("No camera avaliable");

    // Check invalid time.
    if (request.usecSinceEpoch < 0
        && request.usecSinceEpoch != nx::api::ImageRequest::kLatestThumbnail)
    {
        return lit("Invalid time");
    }
    if (request.ignoreExternalArchive
        && request.usecSinceEpoch != nx::api::ImageRequest::kLatestThumbnail
        && request.usecSinceEpoch != DATETIME_NOW)
    {
        return QString("'%1' applies only for \"latest\" timestamp")
            .arg(kIgnoreExternalArchive);
    }

    const auto& size = request.size;
    if (size.height() > 0 && size.height() < nx::api::CameraImageRequest::kMinimumSize)
        return QString("Height cannot be less than %1").arg(nx::api::CameraImageRequest::kMinimumSize);
    if (size.width() > 0 && size.width() < nx::api::CameraImageRequest::kMinimumSize)
        return QString("Width cannot be less than %1").arg(nx::api::CameraImageRequest::kMinimumSize);
    if (size.width() > 0 && size.height() < 0)
        return lit("Width cannot be specified without specifying height");

    return boost::none;
}
