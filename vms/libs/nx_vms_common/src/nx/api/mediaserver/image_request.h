// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QRectF>
#include <QtCore/QSize>

#include <core/resource/resource_fwd.h>
#include <nx/reflect/enum_instrument.h>

namespace nx::api {

struct ImageRequest
{
    /**
     * Getting a thumbnail at the exact timestamp is costly, so we can round it to the nearest
     * keyframe.
     */
    enum class RoundMethod
    {
        /** Get the thumbnail from the nearest keyframe before the given time. */
        iFrameBefore,

        /** Get the thumbnail as near to given time as possible. */
        precise,

        /** Get the thumbnail from the nearest keyframe after the given time. */
        iFrameAfter,
    };

    template<typename Visitor>
    friend constexpr auto nxReflectVisitAllEnumItems(RoundMethod*, Visitor&& visitor)
    {
        using Item = nx::reflect::enumeration::Item<RoundMethod>;
        return visitor(
            Item{RoundMethod::iFrameBefore, "before"},
            Item{RoundMethod::precise, "precise"},
            Item{RoundMethod::precise, "exact"}, //< deprecated
            Item{RoundMethod::iFrameAfter, "after"}
        );
    }

    enum class ThumbnailFormat
    {
        png,
        jpg,
        tif,
        raw, /**< Raw video frame. Makes the request much more lightweight for Edge servers. */
    };

    template<typename Visitor>
    friend constexpr auto nxReflectVisitAllEnumItems(ThumbnailFormat*, Visitor&& visitor)
    {
        using Item = nx::reflect::enumeration::Item<ThumbnailFormat>;
        return visitor(
            Item{ThumbnailFormat::png, "png"},
            Item{ThumbnailFormat::jpg, "jpg"},
            Item{ThumbnailFormat::jpg, "jpeg"}, //< deprecated
            Item{ThumbnailFormat::tif, "tif"},
            Item{ThumbnailFormat::tif, "tiff"}, //< deprecated
            Item{ThumbnailFormat::raw, "raw"}
        );
    }

    NX_REFLECTION_ENUM_CLASS_IN_CLASS(AspectRatio,
        auto_, /**< Use aspect ratio from camera settings (if any). */
        source /**< Use actual image aspect ratio. */
    )

    /**
     * Currently this is used only in camera image requests.
     */
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(StreamSelectionMode,
        auto_,
        forcedPrimary,
        forcedSecondary,
        sameAsMotion,
        sameAsAnalytics
    )

    static const qint64 kLatestThumbnail = -1;
    static const int kDefaultRotation = -1;

    /**
     * Timestamp of the image. A negative value means "latest". Can have the special value
     * DATETIME_NOW.
     */
    qint64 usecSinceEpoch = kLatestThumbnail;

    /**
     * If enabled, images requested with "Latest" option will not be tried to download from an
     * external archive like NVR.
     */
    bool ignoreExternalArchive = false;

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

    AspectRatio aspectRatio = AspectRatio::auto_;

    /**
     * Whether it's allowed to get the closest available frame if there's no archive
     * at the requested time.
     */
    bool tolerant = false;

    /** Crop image. Values in range [0..1] */
    QRectF crop;

    StreamSelectionMode streamSelectionMode = StreamSelectionMode::auto_;
    QnUuid objectTrackId;
};

struct ResourceImageRequest: ImageRequest
{
    QnResourcePtr resource;
};

struct NX_VMS_COMMON_API CameraImageRequest: ImageRequest
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

} // namespace nx::api

Q_DECLARE_METATYPE(nx::api::ImageRequest::StreamSelectionMode)
