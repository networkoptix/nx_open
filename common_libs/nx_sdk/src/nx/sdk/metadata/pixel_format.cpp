#include "pixel_format.h"

#include <map>

#include <nx/kit/debug.h>

namespace nx {
namespace sdk {
namespace metadata {

#define PIXEL_FORMAT_ENTRY(VALUE) {#VALUE, UncompressedVideoFrame::PixelFormat::VALUE}
static const std::map<std::string, UncompressedVideoFrame::PixelFormat> pixelFormatByString{
    PIXEL_FORMAT_ENTRY(yuv420),
    PIXEL_FORMAT_ENTRY(argb),
    PIXEL_FORMAT_ENTRY(abgr),
    PIXEL_FORMAT_ENTRY(rgba),
    PIXEL_FORMAT_ENTRY(bgra),
    PIXEL_FORMAT_ENTRY(rgb),
    PIXEL_FORMAT_ENTRY(bgr),
};
#undef PIXEL_FORMAT_ENTRY

bool pixelFormatFromStdString(const std::string& s, UncompressedVideoFrame::PixelFormat* outValue)
{
    const auto& it = pixelFormatByString.find(s);
    if (it == pixelFormatByString.cend())
        return false;

    if (outValue)
        *outValue = it->second;
    return true;
}

std::string pixelFormatToStdString(UncompressedVideoFrame::PixelFormat value)
{
    for (const auto& entry: pixelFormatByString)
    {
        if (entry.second == value)
            return entry.first;
    }
    NX_PRINT << "INTERNAL ERROR: String value for pixel format " << (int) value
        << " not registered in " << nx::kit::debug::relativeSrcFilename(__FILE__);
    return "";
}

std::string allPixelFormatsToStdString(const std::string& separator)
{
    std::string result;
    for (const auto& entry: pixelFormatByString)
    {
        if (!result.empty())
            result += separator;
        result += entry.first;
    }
    return result;
}

} // namespace metadata
} // namespace sdk
} // namespace nx
