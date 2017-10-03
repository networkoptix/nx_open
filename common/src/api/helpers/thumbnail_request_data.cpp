#include "thumbnail_request_data.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/fusion/model_functions.h>
#include <nx/utils/string.h>
#include <nx/utils/datetime.h>

#include <api/helpers/camera_id_helper.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(QnThumbnailRequestData, RoundMethod,
    (QnThumbnailRequestData::KeyFrameBeforeMethod, "before")
    (QnThumbnailRequestData::PreciseMethod, "precise")
    (QnThumbnailRequestData::PreciseMethod, "exact")
    (QnThumbnailRequestData::KeyFrameAfterMethod, "after")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(QnThumbnailRequestData, ThumbnailFormat,
    (QnThumbnailRequestData::JpgFormat, "jpeg")
    (QnThumbnailRequestData::JpgFormat, "jpg")
    (QnThumbnailRequestData::TiffFormat, "tiff")
    (QnThumbnailRequestData::TiffFormat, "tif")
    (QnThumbnailRequestData::PngFormat, "png")
    (QnThumbnailRequestData::RawFormat, "raw")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(QnThumbnailRequestData, AspectRatio,
    (QnThumbnailRequestData::AutoAspectRatio, "auto")
    (QnThumbnailRequestData::SourceAspectRatio, "source")
)

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

QnThumbnailRequestData::QnThumbnailRequestData():
    QnMultiserverRequestData(),
    camera(),
    msecSinceEpoch(kLatestThumbnail),
    rotation(kDefaultRotation),
    size(),
    imageFormat(JpgFormat),
    roundMethod(KeyFrameAfterMethod), //< round after is better then before by default for most situations
    aspectRatio(AutoAspectRatio)
{
}

bool QnThumbnailRequestData::isSpecialTimeValue(qint64 value)
{
    return value < 0 || value == DATETIME_NOW;
}

void QnThumbnailRequestData::loadFromParams(QnResourcePool* resourcePool,
    const QnRequestParamList& params)
{
    QnMultiserverRequestData::loadFromParams(resourcePool, params);

    camera = nx::camera_id_helper::findCameraByFlexibleIds(
        resourcePool,
        /*outNotFoundCameraId*/ nullptr,
        params.toHash(),
        {kCameraIdParam, kDeprecatedPhysicalIdParam, kDeprecatedMacParam})
        .dynamicCast<QnVirtualCameraResource>();

    if (params.contains(kTimeParam))
    {
        QString timeValue = params.value(kTimeParam);
        if (timeValue.toLower() == kLatestTimeValue)
            msecSinceEpoch = kLatestThumbnail;
        else
            msecSinceEpoch = nx::utils::parseDateTimeMsec(timeValue);
    }

    rotation = QnLexical::deserialized<int>(params.value(kRotateParam), rotation);
    size.setHeight(QnLexical::deserialized<int>(params.value(kHeightParam), size.height()));
    if (params.contains(kWidthParam))
        size.setWidth(QnLexical::deserialized<int>(params.value(kWidthParam), size.width()));
    else
        size.setWidth(QnLexical::deserialized<int>(params.value(kDeprecatedWidthParam), size.width()));
    imageFormat = QnLexical::deserialized<ThumbnailFormat>(
        params.value(kImageFormatParam), /*defaultValue*/ imageFormat);
    roundMethod = QnLexical::deserialized<RoundMethod>(
        params.value(kRoundMethodParam), /*defaultValue*/ roundMethod);
    aspectRatio = QnLexical::deserialized<AspectRatio>(
        params.value(kAspectRatioParam), /*defaultValue*/ aspectRatio);
}

QnRequestParamList QnThumbnailRequestData::toParams() const
{
    QnRequestParamList result = QnMultiserverRequestData::toParams();

    result.insert(kCameraIdParam,
        QnLexical::serialized(camera ? camera->getId().toString() : QString()));
    result.insert(kTimeParam,
        isSpecialTimeValue(msecSinceEpoch)
        ? kLatestTimeValue
        : QnLexical::serialized(msecSinceEpoch));
    result.insert(kRotateParam, QnLexical::serialized(rotation));
    result.insert(kHeightParam, QnLexical::serialized(size.height()));
    result.insert(kWidthParam, QnLexical::serialized(size.width()));
    result.insert(kImageFormatParam, QnLexical::serialized(imageFormat));
    result.insert(kRoundMethodParam, QnLexical::serialized(roundMethod));
    return result;
}

bool QnThumbnailRequestData::isValid() const
{
    if (!camera)
        return false;

    // Check invalid time.
    if (msecSinceEpoch < 0 && msecSinceEpoch != kLatestThumbnail)
        return false;

    if (size.height() > 0 && size.height() < kMinimumSize)
        return false;

    // Width cannot be specified without specifying height.
    if (size.width() > 0 && (size.width() < kMinimumSize || size.height() < 0))
        return false;

    return true;
}
