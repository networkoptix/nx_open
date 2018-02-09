#include "image_request.h"

#include <nx/fusion/model_functions.h>

#include <nx/utils/datetime.h>

namespace nx {
namespace api {

bool CameraImageRequest::isSpecialTimeValue(qint64 value)
{
    return value < 0 || value == DATETIME_NOW;
}

} // namespace api
} // namespace nx

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::api::ImageRequest, RoundMethod,
    (nx::api::ImageRequest::RoundMethod::iFrameBefore, "before")
    (nx::api::ImageRequest::RoundMethod::precise, "precise")
    (nx::api::ImageRequest::RoundMethod::precise, "exact")
    (nx::api::ImageRequest::RoundMethod::iFrameAfter, "after")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::api::ImageRequest, ThumbnailFormat,
    (nx::api::ImageRequest::ThumbnailFormat::jpg, "jpeg")
    (nx::api::ImageRequest::ThumbnailFormat::jpg, "jpg")
    (nx::api::ImageRequest::ThumbnailFormat::tiff, "tiff")
    (nx::api::ImageRequest::ThumbnailFormat::tiff, "tif")
    (nx::api::ImageRequest::ThumbnailFormat::png, "png")
    (nx::api::ImageRequest::ThumbnailFormat::raw, "raw")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::api::ImageRequest, AspectRatio,
    (nx::api::ImageRequest::AspectRatio::custom, "auto")
    (nx::api::ImageRequest::AspectRatio::custom, "custom")
    (nx::api::ImageRequest::AspectRatio::source, "source")
)
