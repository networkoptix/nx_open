#include "thumbnail_request_data.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/fusion/model_functions.h>
#include <utils/common/string.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(QnThumbnailRequestData::RoundMethod,
                                          (QnThumbnailRequestData::KeyFrameBeforeMethod,    "before")
                                          (QnThumbnailRequestData::PreciseMethod,           "precise")
                                          (QnThumbnailRequestData::PreciseMethod,           "exact")
                                          (QnThumbnailRequestData::KeyFrameAfterMethod,     "after")
                                          );

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(QnThumbnailRequestData::ThumbnailFormat,
                                          (QnThumbnailRequestData::JpgFormat,               "jpeg")
                                          (QnThumbnailRequestData::JpgFormat,               "jpg")
                                          (QnThumbnailRequestData::TiffFormat,              "tiff")
                                          (QnThumbnailRequestData::TiffFormat,              "tif")
                                          (QnThumbnailRequestData::PngFormat,               "png")
                                          (QnThumbnailRequestData::RawFormat,               "raw")
                                          );

namespace
{
    const QString physicalIdKey     (lit("physicalId"));
    const QString macKey            (lit("mac"));
    const QString cameraIdKey       (lit("cameraId"));
    const QString timeKey           (lit("time"));
    const QString rotationKey       (lit("rotate"));
    const QString heightKey         (lit("height"));
    const QString widthKey          (lit("widht"));
    const QString imageFormatKey    (lit("imageFormat"));
    const QString roundMethodKey    (lit("method"));

    const QString kLatestTimeValue  (lit("latest"));
    const int kMinimumSize = 128;
}

QnThumbnailRequestData::QnThumbnailRequestData()
    : QnMultiserverRequestData()
    , camera()
    , msecSinceEpoch(kLatestThumbnail)
    , rotation(kDefaultRotation)
    , size()
    , imageFormat(JpgFormat)
    , roundMethod(KeyFrameBeforeMethod)
{

}

bool QnThumbnailRequestData::isSpecialTimeValue( qint64 value )
{
    return value < 0 || value == DATETIME_NOW;
}


void QnThumbnailRequestData::loadFromParams( const QnRequestParamList& params )
{
    QnMultiserverRequestData::loadFromParams(params);

    if (params.contains(physicalIdKey))
        camera = qnResPool->getNetResourceByPhysicalId(params.value(physicalIdKey)).dynamicCast<QnVirtualCameraResource>();
    else if (params.contains(macKey))
        camera = qnResPool->getResourceByMacAddress(params.value(macKey)).dynamicCast<QnVirtualCameraResource>();
    else if (params.contains(cameraIdKey))
        camera = qnResPool->getResourceById(QnUuid::fromStringSafe(params.value(cameraIdKey))).dynamicCast<QnVirtualCameraResource>();

    if (params.contains(timeKey))
    {
        QString timeValue = params.value(timeKey);
        if (timeValue == kLatestTimeValue)
            msecSinceEpoch = kLatestThumbnail;
        else
            msecSinceEpoch = parseDateTimeMsec(timeValue);
    }

    rotation      = QnLexical::deserialized<int>(params.value(rotationKey),                 rotation);
    size.setHeight( QnLexical::deserialized<int>(params.value(heightKey),                   size.height()));
    size.setWidth ( QnLexical::deserialized<int>(params.value(widthKey),                    size.width()));
    imageFormat   = QnLexical::deserialized<ThumbnailFormat>(params.value(imageFormatKey),  imageFormat);
    roundMethod   = QnLexical::deserialized<RoundMethod>(params.value(roundMethodKey),      roundMethod);
}

QnRequestParamList QnThumbnailRequestData::toParams() const
{
    QnRequestParamList result = QnMultiserverRequestData::toParams();

    result.insert(cameraIdKey,      QnLexical::serialized(camera ? camera->getId().toString() : QString()));
    result.insert(timeKey,          isSpecialTimeValue(msecSinceEpoch)
        ? kLatestTimeValue
        : QnLexical::serialized(msecSinceEpoch));
    result.insert(rotationKey,      QnLexical::serialized(rotation));
    result.insert(heightKey,        QnLexical::serialized(size.height()));
    result.insert(widthKey,         QnLexical::serialized(size.width()));
    result.insert(imageFormatKey,   QnLexical::serialized(imageFormat));
    result.insert(roundMethodKey,   QnLexical::serialized(roundMethod));
    return result;
}

bool QnThumbnailRequestData::isValid() const
{
    if (!camera)
        return false;

    /* Check invalid time. */
    if (msecSinceEpoch < 0 && msecSinceEpoch != kLatestThumbnail)
        return false;

    if (size.height() > 0 && size.height() < kMinimumSize)
        return false;

    /* Width cannot be specified without specifying height. */
    if (size.width() > 0 && (size.width() < kMinimumSize || size.height() < 0))
        return false;

    return true;
}

