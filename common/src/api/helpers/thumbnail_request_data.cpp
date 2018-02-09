#include "thumbnail_request_data.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/utils/string.h>
#include <nx/utils/datetime.h>

#include <api/helpers/camera_id_helper.h>

#include <nx/fusion/model_functions.h>

namespace {

static const QString kDeprecatedPhysicalIdParam = lit("physicalId");
static const QString kDeprecatedMacParam = lit("mac");
static const QString kCameraIdParam = lit("cameraId");
static const QString kTimeParam = lit("time");
static const QString kRotateParam = lit("rotate");
static const QString kHeightParam = lit("height");
static const QString kDeprecatedWidthParam = lit("widht");
static const QString kWidthParam = lit("width");
static const QString kImageFormatParam = lit("imageFormat");
static const QString kRoundMethodParam = lit("method");
static const QString kAspectRatioParam = lit("aspectRatio");

static const QString kLatestTimeValue = lit("latest");

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
            request.msecSinceEpoch = nx::api::ImageRequest::kLatestThumbnail;
        else
            request.msecSinceEpoch = nx::utils::parseDateTimeMsec(timeValue);
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
}

QnRequestParamList QnThumbnailRequestData::toParams() const
{
    QnRequestParamList result = QnMultiserverRequestData::toParams();

    result.insert(kCameraIdParam,
        QnLexical::serialized(request.camera ? request.camera->getId().toString() : QString()));
    result.insert(kTimeParam,
        nx::api::CameraImageRequest::isSpecialTimeValue(request.msecSinceEpoch)
        ? kLatestTimeValue
        : QnLexical::serialized(request.msecSinceEpoch));
    result.insert(kRotateParam, QnLexical::serialized(request.rotation));
    result.insert(kHeightParam, QnLexical::serialized(request.size.height()));
    result.insert(kWidthParam, QnLexical::serialized(request.size.width()));
    result.insert(kImageFormatParam, QnLexical::serialized(request.imageFormat));
    result.insert(kRoundMethodParam, QnLexical::serialized(request.roundMethod));
    result.insert(kAspectRatioParam, QnLexical::serialized(request.aspectRatio));
    return result;
}

boost::optional<QString> QnThumbnailRequestData::getError() const
{
    if (!request.camera)
        return lit("No camera avaliable");

    // Check invalid time.
    if (request.msecSinceEpoch < 0
        && request.msecSinceEpoch != nx::api::ImageRequest::kLatestThumbnail)
    {
        return lit("Invalid time");
    }

    const auto& size = request.size;
    if (size.height() > 0 && size.height() < nx::api::CameraImageRequest::kMinimumSize)
        return lit("Invalid height");

    if (size.width() > 0
        && (size.width() < nx::api::CameraImageRequest::kMinimumSize || size.height() < 0))
    {
        return lit("Width cannot be specified without specifying height");
    }

    return boost::none;
}
