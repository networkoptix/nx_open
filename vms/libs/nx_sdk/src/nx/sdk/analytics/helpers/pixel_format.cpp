#include "pixel_format.h"

#include <array>

#include <nx/kit/debug.h>

namespace nx {
namespace sdk {
namespace analytics {

using PixelFormat = IUncompressedVideoFrame::PixelFormat;
using PixelFormatDescriptors = std::array<PixelFormatDescriptor, (int) PixelFormat::count>;

static PixelFormatDescriptors initPixelFormatDescriptors()
{
    PixelFormatDescriptors result;

    #define INIT_PIXEL_FORMAT_DESCRIPTOR(PIXEL_FORMAT, ...) \
        result[(int) PixelFormat::PIXEL_FORMAT] = \
            {PixelFormat::PIXEL_FORMAT, #PIXEL_FORMAT, __VA_ARGS__}

    //                                   planeCount
    //                                      lumaBitsPerPixel
    //                                          chromaHeightFactor
    //                                             chromaWidthFactor
    INIT_PIXEL_FORMAT_DESCRIPTOR(yuv420, 3,  8, 2, 2);
    INIT_PIXEL_FORMAT_DESCRIPTOR(argb,   1, 32, 1, 1);
    INIT_PIXEL_FORMAT_DESCRIPTOR(abgr,   1, 32, 1, 1);
    INIT_PIXEL_FORMAT_DESCRIPTOR(rgba,   1, 32, 1, 1);
    INIT_PIXEL_FORMAT_DESCRIPTOR(bgra,   1, 32, 1, 1);
    INIT_PIXEL_FORMAT_DESCRIPTOR(rgb,    1, 24, 1, 1);
    INIT_PIXEL_FORMAT_DESCRIPTOR(bgr,    1, 24, 1, 1);

    #undef INIT_PIXEL_FORMAT_DESCRIPTOR

    return result;
}

static const PixelFormatDescriptors pixelFormatDescriptors = initPixelFormatDescriptors();

const PixelFormatDescriptor* getPixelFormatDescriptor(PixelFormat pixelFormat)
{
    if ((int) pixelFormat < 0 || (int) pixelFormat >= pixelFormatDescriptors.size())
    {
        NX_PRINT << "INTERNAL ERROR: PixelFormat value " << (int) pixelFormat << " not registered";
        return nullptr;
    }

    return &pixelFormatDescriptors[(int) pixelFormat];
}

std::vector<PixelFormat> getAllPixelFormats()
{
    std::vector<PixelFormat> result;
    for (const auto& descriptor: pixelFormatDescriptors)
        result.push_back(descriptor.pixelFormat);
    return result;
}

bool pixelFormatFromStdString(const std::string& s, PixelFormat* outPixelFormat)
{
    for (const auto& descriptor: pixelFormatDescriptors)
    {
        if (descriptor.name == s)
        {
            if (outPixelFormat)
                *outPixelFormat = descriptor.pixelFormat;
            return true;
        }
    }
    return false;
}

std::string pixelFormatToStdString(PixelFormat pixelFormat)
{
    if ((int) pixelFormat < 0 || (int) pixelFormat >= (int) pixelFormatDescriptors.size())
    {
        NX_PRINT << "INTERNAL ERROR: PixelFormat value " << (int) pixelFormat << " not registered";
        return "";
    }

    return pixelFormatDescriptors[(int) pixelFormat].name;
}

std::string allPixelFormatsToStdString(const std::string& separator)
{
    std::string result;
    for (const auto& descriptor: pixelFormatDescriptors)
    {
        if (!result.empty())
            result += separator;
        result += descriptor.name;
    }
    return result;
}

} // namespace analytics
} // namespace sdk
} // namespace nx
