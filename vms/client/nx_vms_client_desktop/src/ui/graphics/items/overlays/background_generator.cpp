#include "background_generator.h"

#include <algorithm>

#include <nx/utils/log/assert.h>

// Generate background by applying Gaussian Blur to image containing a centered rectangle.
// Based on linear time algorithm pseudo-code by Ivan Kutskir.
// Specialized for our purposes -- not suitable for general use.

static void gaussianBlur(int width, int height, int maskSize, float* pixels);
static void boxBlur(int width, int height, float* src, float* dest, int radius);
static void boxBlurHorizontal(int width, int height, const float* src, float* dest, int radius);
static void boxBlurVertical(int width, int height, const float* src, float* dest, int radius);
static std::vector<int> fillBoxesForGauss(int numBoxes, int maskSize);

QImage nx::vms::client::desktop::generatePlaceholderBackground(int width,
    int height,
    const QRect& rect,
    int maskSize,
    const QColor& foreground,
    const QColor& background)
{
    NX_ASSERT(rect.x() + rect.width() <= width && rect.y() + rect.height() <= height,
        "Rect out of bounds");

    int size = width * height;
    auto pixels = std::make_unique<float[]>(size);

    std::fill(pixels.get(), pixels.get() + size, 0);

    for (int y = rect.y(); y < rect.bottom() + 1; y++)
        std::fill(&pixels[y * width + rect.x()], &pixels[y * width + rect.right() + 1], 1);

    gaussianBlur(width, height, maskSize, pixels.get());

    constexpr int kAlpha = 40;

    QImage image(width, height, QImage::Format_ARGB32);
    for (int y = 0; y < height; y++)
    {
        auto line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < width; x++)
        {
            auto pixel = pixels[y * width + x];
            pixel = std::clamp(pixel, 0.f, 1.f);

            auto r = pixel * foreground.red() + (1 - pixel) * background.red();
            auto g = pixel * foreground.green() + (1 - pixel) * background.green();
            auto b = pixel * foreground.blue() + (1 - pixel) * background.blue();

            line[x] = qRgba(std::round(r), std::round(g), std::round(b), kAlpha);
        }
    }

    return image;
}

static void gaussianBlur(int width, int height, int maskSize, float* pixels)
{
    int size = width * height;
    auto temp = std::make_unique<float[]>(size);

    constexpr int kNumBoxes = 3;
    auto boxes = fillBoxesForGauss(kNumBoxes, maskSize);

    boxBlur(width, height, pixels, temp.get(), (boxes[0] - 1) / 2);
    boxBlur(width, height, pixels, temp.get(), (boxes[1] - 1) / 2);
    boxBlur(width, height, pixels, temp.get(), (boxes[2] - 1) / 2);
}

static void boxBlur(int width, int height, float* image, float* temp, int radius)
{
    boxBlurHorizontal(width, height, image, temp, radius);
    boxBlurVertical(width, height, temp, image, radius);
}

static void boxBlurHorizontal(int width, int height, const float* src, float* dest, int radius)
{
    float iarr = 1.0 / (2 * radius + 1);

    for (int y = 0; y < height; y++)
    {
        auto first = src[y * width];
        auto last = first;
        auto sum = radius * first;

        for (int x = 0; x < radius + 1; x++)
            sum += src[y * width + x];

        for (int x = 0; x < radius + 1; x++)
        {
            dest[y * width + x] = sum * iarr;
            last = src[y * width + x + radius + 1];
            sum += last - first;
        }

        for (int x = radius + 1; x < width - radius - 1; x++)
        {
            dest[y * width + x] = sum * iarr;
            first = src[y * width + x - radius];
            last = src[y * width + x + radius + 1];
            sum += last - first;
        }

        NX_ASSERT(last == src[(y + 1) * width - 1]);
        for (int x = width - radius - 1; x < width; x++)
        {
            dest[y * width + x] = sum * iarr;
            first = src[y * width + x - radius];
            sum += last - first;
        }
    }
}

static void boxBlurVertical(int width, int height, const float* src, float* dest, int radius)
{
    float iarr = 1.0 / (2 * radius + 1);

    for (int x = 0; x < width; x++)
    {
        auto first = src[x];
        auto last = first;
        auto sum = radius * first;

        for (int y = 0; y < radius + 1; y++)
            sum += src[y * width + x];

        for (int y = 0; y < radius + 1; y++)
        {
            dest[y * width + x] = sum * iarr;
            last = src[(y + radius + 1) * width + x];
            sum += last - first;
        }

        for (int y = radius + 1; y < height - radius - 1; y++)
        {
            dest[y * width + x] = sum * iarr;
            first = src[(y - radius) * width + x];
            last = src[(y + radius + 1) * width + x];
            sum += last - first;
        }

        NX_ASSERT(last == src[(height - 1) * width + x]);
        for (int y = height - radius - 1; y < height; y++)
        {
            dest[y * width + x] = sum * iarr;
            first = src[(y - radius) * width + x];
            sum += last - first;
        }
    }
}

static std::vector<int> fillBoxesForGauss(int numBoxes, int maskSize)
{
    auto sigma = maskSize / 2;
    auto wIdeal = std::sqrt((12 * sigma * sigma / numBoxes) + 1);
    int wl = std::floor(wIdeal);
    if (wl % 2 == 0)
        wl--;
    auto wu = wl + 2;

    auto mIdeal = (12 * sigma * sigma - numBoxes * wl * wl - 4 * numBoxes * wl - 3 * numBoxes)
        / (-4 * wl - 4);

    std::vector<int> sizes;
    for (int i = 0; i < numBoxes; i++)
        sizes.push_back(i < mIdeal ? wl : wu);
    return sizes;
}
