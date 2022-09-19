// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QRectF>
#include <QtCore/QSize>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

class ThumbnailSize: public QSize
{
public:
    using QSize::QSize;
    ThumbnailSize(const QSize& size): QSize(size) {}
};

bool deserialize(QnJsonContext* ctx, const QJsonValue& value, ThumbnailSize* target);

class ThumbnailCrop: public QRectF
{
public:
    ThumbnailCrop() = default;
    ThumbnailCrop(const QRectF& crop): QRectF(crop) {}
};

bool deserialize(QnJsonContext* ctx, const QJsonValue& value, ThumbnailCrop* target);

struct NX_VMS_API ThumbnailFilter
{
    enum class RoundMethod
    {
        /**%apidoc Get the thumbnail from the nearest keyframe before the given time. */
        iFrameBefore,

        /**%apidoc Get the thumbnail as near to given time as possible. */
        precise,

        /**%apidoc Get the thumbnail from the nearest keyframe after the given time. */
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
        auto_, /**< Use aspect ratio from device settings (if any). */
        source /**< Use actual image aspect ratio. */
    );

    NX_REFLECTION_ENUM_CLASS_IN_CLASS(StreamSelectionMode,
        auto_,
        forcedPrimary,
        forcedSecondary,
        sameAsMotion,
        sameAsAnalytics
    );

    static constexpr std::chrono::microseconds kLatestThumbnail{-1};
    static constexpr int kDefaultRotation = -1;
    static constexpr int kMinimumSize = 32;
    static constexpr int kMaximumSize = 4096;

    /**%apidoc Device id. */
    QnUuid id;

    /**%apidoc[opt] Timestamp of the image. A negative value means "latest".
     * %example 1000
     */
    std::chrono::microseconds timestampUs{kLatestThumbnail};

    /**%apidoc[opt]
     * If enabled, images requested with "Latest" option will not be tried to download from an
     * external archive like NVR.
     */
    bool ignoreExternalArchive = false;

    /**%apidoc[opt]
     * Forced rotation. Negative value means take default rotation from the device settings.
     * %example 10
     */
    int rotation = kDefaultRotation;

    /**%apidoc[opt]:string
     * Image size, format `{width}x{height}`. Any valid value must not be less than 32 and not
     * larger than 4096, or it can be less than or equal to 0 to indicate auto-sizing. During
     * auto-sizing if only height or only width is positive then aspect ratio is used to calculate
     * the other value, and if both values are not positive then the original frame size is used.
     */
    ThumbnailSize size;

    /**%apidoc[opt] Resulting image format. Default is `jpg`. */
    ThumbnailFormat format = ThumbnailFormat::jpg;

    /**%apidoc[opt] Method of rounding, influences the precision. Round after is better for most situations. */
    RoundMethod roundMethod = RoundMethod::iFrameAfter;

    /**%apidoc[opt] Aspect ratio. */
    AspectRatio aspectRatio = AspectRatio::auto_;

    /**%apidoc[opt]
     * Whether it's allowed to get the closest available frame if there's no archive
     * at the requested time.
     */
    bool tolerant = false;

    /**%apidoc[opt]:string Crop image, format `{x},{y},{width}x{height}` with values in range [0..1] */
    ThumbnailCrop crop;

    /**%apidoc[opt] Stream choice. */
    StreamSelectionMode streamSelectionMode = StreamSelectionMode::auto_;
};

#define ThumbnailFilter_Fields (id)(timestampUs)(ignoreExternalArchive)(rotation)(size)\
    (format)(roundMethod)(aspectRatio)(tolerant)(crop)(streamSelectionMode)
QN_FUSION_DECLARE_FUNCTIONS(ThumbnailFilter, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ThumbnailFilter, ThumbnailFilter_Fields)

} // namespace nx::vms::api
