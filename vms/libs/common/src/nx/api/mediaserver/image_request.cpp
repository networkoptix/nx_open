#include "image_request.h"

#include <nx/fusion/model_functions.h>

#include <nx/utils/datetime.h>

namespace nx {
namespace api {

bool CameraImageRequest::isSpecialTimeValue(qint64 value)
{
    return value < 0 || value == DATETIME_NOW;
}

QString toString(ImageRequest::StreamSelectionMode value)
{
    return QnLexical::serialized(value);
}

} // namespace api
} // namespace nx

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::api::ImageRequest, RoundMethod,
    (nx::api::ImageRequest::RoundMethod::iFrameBefore, "before")
    (nx::api::ImageRequest::RoundMethod::precise, "precise")
    (nx::api::ImageRequest::RoundMethod::precise, "exact") //< deprecated
    (nx::api::ImageRequest::RoundMethod::iFrameAfter, "after")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::api::ImageRequest, ThumbnailFormat,
    (nx::api::ImageRequest::ThumbnailFormat::jpg, "jpg")
    (nx::api::ImageRequest::ThumbnailFormat::jpg, "jpeg") //< deprecated
    (nx::api::ImageRequest::ThumbnailFormat::tif, "tif")
    (nx::api::ImageRequest::ThumbnailFormat::tif, "tiff") //< deprecated
    (nx::api::ImageRequest::ThumbnailFormat::png, "png")
    (nx::api::ImageRequest::ThumbnailFormat::raw, "raw")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::api::ImageRequest, AspectRatio,
    (nx::api::ImageRequest::AspectRatio::auto_, "auto")
    (nx::api::ImageRequest::AspectRatio::source, "source")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::api::ImageRequest, StreamSelectionMode,
    (nx::api::ImageRequest::StreamSelectionMode::auto_, "auto")
    (nx::api::ImageRequest::StreamSelectionMode::forcedPrimary, "forcedPrimary")
    (nx::api::ImageRequest::StreamSelectionMode::forcedSecondary, "forcedSecondary")
    (nx::api::ImageRequest::StreamSelectionMode::sameAsAnalytics, "sameAsAnalytics")
    (nx::api::ImageRequest::StreamSelectionMode::sameAsMotion, "sameAsMotion")
)
