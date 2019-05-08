#pragma once

#include <string>
#include <vector>

#include <nx/sdk/analytics/i_uncompressed_video_frame.h>

namespace nx {
namespace sdk {
namespace analytics {

/** @param outValue Can be null if the value is not needed. */
bool pixelFormatFromStdString(
    const std::string& s, IUncompressedVideoFrame::PixelFormat* outPixelFormat);

/**
 * @return String representation, or, if the value is unknown, a string like `PixelFormat(#)`,
 * where `#` is the integer value.
 */
std::string pixelFormatToStdString(IUncompressedVideoFrame::PixelFormat pixelFormat);

std::string allPixelFormatsToStdString(const std::string& separator);

struct PixelFormatDescriptor
{
    IUncompressedVideoFrame::PixelFormat pixelFormat;
    std::string name;

    int planeCount;
    int lumaBitsPerPixel;
    int chromaHeightFactor; /**< Number of original image lines per 1 chroma plane pixel. */
    int chromaWidthFactor; /**< Number of original image columns per 1 chroma plane pixel. */
};

/** @return Null if not found, after failing an assertion. */
const PixelFormatDescriptor* getPixelFormatDescriptor(
    IUncompressedVideoFrame::PixelFormat pixelFormat);

std::vector<IUncompressedVideoFrame::PixelFormat> getAllPixelFormats();

} // namespace analytics
} // namespace sdk
} // namespace nx
