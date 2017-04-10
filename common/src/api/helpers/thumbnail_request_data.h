#pragma once

#include <QtCore/QSize>

#include <api/helpers/multiserver_request_data.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/request_param.h>
#include <nx/fusion/model_functions_fwd.h>

struct QnThumbnailRequestData: public QnMultiserverRequestData
{
    /**
     * Getting thumbnail from fixed time is costly, so we can round it to the nearest keyframe.
     */
    enum RoundMethod
    {
        KeyFrameBeforeMethod, //< Get thumbnail from the nearest keyframe before the given time.
        PreciseMethod, //< Get thumbnail as near to given time as possible.
        KeyFrameAfterMethod, //< Get thumbnail from the nearest keyframe after the given time.
    };

    enum ThumbnailFormat
    {
        PngFormat,
        JpgFormat,
        TiffFormat,
        RawFormat, //< Raw video frame. Makes the request much more lightweight for Edge servers.
    };

    QnThumbnailRequestData();

    void loadFromParams(QnResourcePool* resourcePool, const QnRequestParamList& params);
    QnRequestParamList toParams() const;
    bool isValid() const;

    /** Check if value is "special" - DATETIME_NOW or negative. */
    static bool isSpecialTimeValue(qint64 value);

    static const qint64 kLatestThumbnail = -1;
    static const int kDefaultRotation = -1;
    static const int kMinimumSize = 128;

    /** Target camera. */
    QnVirtualCameraResourcePtr camera;

    /** Timestamp. Negative value means 'latest'. Can take the special value DATETIME_NOW. */
    qint64 msecSinceEpoch;

    /** Forced rotation. Negative value means take default rotation from camera settings. */
    int rotation;

    /**
     * Image size. If have only height or only width, aspect ratio will be used. Any value must be
     * not less than 128.
     */
    QSize size;

    /** Result image format. */
    ThumbnailFormat imageFormat;

    /** Precision option. */
    RoundMethod roundMethod;
};

#define QN_THUMBNAIL_ENUM_TYPES \
    (QnThumbnailRequestData::RoundMethod)\
    (QnThumbnailRequestData::ThumbnailFormat)\

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(QN_THUMBNAIL_ENUM_TYPES, (lexical))
