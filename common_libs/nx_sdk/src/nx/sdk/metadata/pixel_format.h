#pragma once

#include <string>
#include <vector>

#include <nx/sdk/metadata/uncompressed_video_frame.h>

namespace nx {
namespace sdk {
namespace metadata {

/** @param outValue Can be null if the value is not needed. */
bool pixelFormatFromStdString(
    const std::string& s, UncompressedVideoFrame::PixelFormat* outPixelFormat);

/** @return Emtpy string if not found, after logging an error. */
std::string pixelFormatToStdString(UncompressedVideoFrame::PixelFormat pixelFormat);

std::string allPixelFormatsToStdString(const std::string& separator);

struct PixelFormatDescriptor
{
    UncompressedVideoFrame::PixelFormat pixelFormat;
    std::string name;

    int planeCount;
    int lumaBitsPerPixel;
    int chromaHeightFactor; /**< Number of original image lines per 1 chroma plane pixel. */
    int chromaWidthFactor; /**< Number of original image columns per 1 chroma plane pixel. */
};

/** @return Null if not found, after logging an error. */
const PixelFormatDescriptor* getPixelFormatDescriptor(
    UncompressedVideoFrame::PixelFormat pixelFormat);

std::vector<UncompressedVideoFrame::PixelFormat> getAllPixelFormats();

} // namespace metadata
} // namespace sdk
} // namespace nx
