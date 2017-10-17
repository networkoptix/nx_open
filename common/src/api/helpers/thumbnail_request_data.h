#pragma once

#include <QtCore/QSize>

#include <api/helpers/multiserver_request_data.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/request_param.h>
#include <nx/fusion/model_functions_fwd.h>

struct QnThumbnailRequestData: public QnMultiserverRequestData
{
    /**
     * Getting a thumbnail at the exact timestamp is costly, so we can round it to the nearest keyframe.
     */
    enum RoundMethod
    {
        KeyFrameBeforeMethod, //< Get the thumbnail from the nearest keyframe before the given time.
        PreciseMethod, //< Get the thumbnail as near to given time as possible.
        KeyFrameAfterMethod, //< Get the thumbnail from the nearest keyframe after the given time.
    };

    enum ThumbnailFormat
    {
        PngFormat,
        JpgFormat,
        TiffFormat,
        RawFormat, //< Raw video frame. Makes the request much more lightweight for Edge servers.
    };

    enum AspectRatio
    {
        AutoAspectRatio,
        SourceAspectRatio,
    };

    QnThumbnailRequestData();

    void loadFromParams(QnResourcePool* resourcePool, const QnRequestParamList& params);
    QnRequestParamList toParams() const;
    bool isValid() const;

    /** Check if value is "special" - DATETIME_NOW or negative. */
    static bool isSpecialTimeValue(qint64 value);

    static const qint64 kLatestThumbnail = -1;
    static const int kDefaultRotation = -1;
    static const int kMinimumSize = 32;
    static const int kMaximumSize = 4096;

    /** Target camera. */
    QnVirtualCameraResourcePtr camera;

    /**
     * Timestamp of the image. A negative value means "latest" Can have the special value
     * DATETIME_NOW.
     */
    qint64 msecSinceEpoch;

    /** Forced rotation. Negative value means take default rotation from the camera settings. */
    int rotation;

    /**
     * Image size. If the image has only height or only width, aspect ratio will be used. Any value
     * must be not less than 128, or -1. If the width is not -1, the height should not be -1.
     */
    QSize size;

    /** Resulting image format. */
    ThumbnailFormat imageFormat;

    /** Method of rounding, influences the precision. */
    RoundMethod roundMethod;

    AspectRatio aspectRatio;
};

#define QN_THUMBNAIL_ENUM_TYPES \
    (QnThumbnailRequestData::RoundMethod)\
    (QnThumbnailRequestData::ThumbnailFormat)\
    (QnThumbnailRequestData::AspectRatio)\

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(QN_THUMBNAIL_ENUM_TYPES, (lexical))
