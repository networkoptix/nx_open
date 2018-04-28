#pragma once

#include <string>

#include <nx/sdk/metadata/uncompressed_video_frame.h>

namespace nx {
namespace sdk {
namespace metadata {

/** @param outValue Can be null if the value is not needed. */
bool pixelFormatFromStdString(const std::string& s, UncompressedVideoFrame::PixelFormat* outValue);

/** @return Emtpy string if not found, after logging an error. */
std::string pixelFormatToStdString(UncompressedVideoFrame::PixelFormat value);

std::string allPixelFormatsToStdString(const std::string& separator);

} // namespace metadata
} // namespace sdk
} // namespace nx
