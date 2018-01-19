#pragma once

#include <QtCore/QSize>

#include <core/resource/resource_fwd.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace api {

struct ImageRequest
{
    /**
     * Getting a thumbnail at the exact timestamp is costly, so we can round it to the nearest
     * keyframe.
     */
    enum class RoundMethod
    {
        iFrameBefore,   //< Get the thumbnail from the nearest keyframe before the given time.
        precise,        //< Get the thumbnail as near to given time as possible.
        iFrameAfter,    //< Get the thumbnail from the nearest keyframe after the given time.
    };

    enum class ThumbnailFormat
    {
        png,
        jpg,
        tiff,
        raw, //< Raw video frame. Makes the request much more lightweight for Edge servers.
    };

    enum class AspectRatio
    {
        custom, //< Resize to camera's custom aspect ratio (if set).
        source, //< Use actual image aspect ratio.
    };

    static const qint64 kLatestThumbnail = -1;
    static const int kDefaultRotation = -1;

    /**
     * Timestamp of the image. A negative value means "latest" Can have the special value
     * DATETIME_NOW.
     */
    qint64 msecSinceEpoch = kLatestThumbnail;

    /** Forced rotation. Negative value means take default rotation from the camera settings. */
    int rotation = kDefaultRotation;

    /**
     * Image size. If the image has only height or only width, aspect ratio will be used. Any value
     * must be not less than 128, or -1. If the width is not -1, the height should not be -1.
     */
    QSize size;

    /** Resulting image format. */
    ThumbnailFormat imageFormat = ThumbnailFormat::jpg;

    /** Method of rounding, influences the precision. Round after is better for most situations. */
    RoundMethod roundMethod = RoundMethod::iFrameAfter;

    AspectRatio aspectRatio = AspectRatio::custom;
};

struct ResourceImageRequest: ImageRequest
{
    QnResourcePtr resource;
};

struct CameraImageRequest: ImageRequest
{
    QnVirtualCameraResourcePtr camera;

    CameraImageRequest() = default;

    CameraImageRequest(const QnVirtualCameraResourcePtr& camera, const ImageRequest& request):
        ImageRequest(request),
        camera(camera)
    {
    }

    /** Check if value is "special" - DATETIME_NOW or negative. */
    static bool isSpecialTimeValue(qint64 value);

    static const int kMinimumSize = 32;
    static const int kMaximumSize = 4096;
};

} // namespace api
} // namespace nx

#define QN_THUMBNAIL_ENUM_TYPES \
    (nx::api::ImageRequest::RoundMethod)\
    (nx::api::ImageRequest::ThumbnailFormat)\
    (nx::api::ImageRequest::AspectRatio)\

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(QN_THUMBNAIL_ENUM_TYPES, (lexical))
