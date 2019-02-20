#include "blur_filter.h"

#include <cmath>

#include <QtGui/QImage>
#include <cmath>

namespace nx::utils::graphics {

BlurFilter::BlurFilter(int blurWidth):
    m_kernelWidth(blurWidth),
    m_kernel(createKernel(blurWidth))
{
}

void BlurFilter::filterImage(QImage& image)
{
    filterImage(image, &BlurFilter::filter1DARGB);
}

void BlurFilter::filterImageAlpha(QImage& image)
{
    // Force Alpha channel.
    if (image.format() != QImage::Format_ARGB32 && image.format() != QImage::Format_ARGB32_Premultiplied)
    {
        image = image.convertToFormat(QImage::Format_ARGB32);
    }

    filterImage(image, &BlurFilter::filter1DAlpha);
}


void BlurFilter::filterImage(QImage& image,
    void (BlurFilter::* filter1D)(QImage image, QImage& blurred) const)
{
    if (image.format() != QImage::Format_RGB32 && image.format() != QImage::Format_ARGB32
        && image.format() != QImage::Format_ARGB32_Premultiplied)
    {
        image = image.convertToFormat(QImage::Format_ARGB32);
    }

    QImage blurred;
    (this->*filter1D)(image, blurred); //< Filter horizontally.

    image = blurred.transformed(QTransform().rotate(90));
    (this->*filter1D)(image, blurred); //< Filter vertically.

    image = blurred.transformed(QTransform().rotate(-90));
}

void BlurFilter::filter1DARGB(QImage image, QImage& blurred) const
{
    blurred = image.copy();
    for(int line = 0; line < image.height(); line++)
    {
        const uint32_t* pixels = (const uint32_t*) image.constScanLine(line);
        uint32_t* blurredPixels = (uint32_t*) blurred.scanLine(line);

        float c1, c2, c3, c4;

        for (int i = m_kernelWidth; i < image.width() - m_kernelWidth - 1; i++)
        {
            c1 = c2 = c3 = c4 = 0;

            for (int j = -m_kernelWidth; j < m_kernelWidth + 1; j++)
            {
                const uint32_t pixel = pixels[i + j];
                const float kernel = m_kernel[j + m_kernelWidth];

                c1 += kernel * (pixel & 0xff);
                c2 += kernel * (pixel & 0xff00);
                c3 += kernel * (pixel & 0xff0000);
                c4 += kernel * (pixel & 0xff000000);
            }

            blurredPixels[i] = (uint32_t)c1 + (0xff00 & (uint32_t)c2)
                + (0xff0000 & (uint32_t)c3) + (0xff000000 & (uint32_t)c4);
        }
    }
}

void BlurFilter::filter1DAlpha(QImage image, QImage& blurred) const
{
    blurred = image.copy();
    for(int line = 0; line < image.height(); line++)
    {
        const uint32_t* pixels = (const uint32_t*) image.constScanLine(line);
        uint32_t* blurredPixels = (uint32_t*) blurred.scanLine(line);

        for (int i = m_kernelWidth; i < image.width() - m_kernelWidth - 1; i++)
        {
            float a = 0;

            for (int j = -m_kernelWidth; j < m_kernelWidth + 1; j++)
                a += pixels[i + j] * m_kernel[j + m_kernelWidth];

            blurredPixels[i] = 0xff000000 & (uint32_t)a;
        }
    }
}

std::vector<float> BlurFilter::createKernel(int size) const
{
    std::vector<float> kernel(2 * size + 1);
    float sum = 0;

    for (int i = 0; i <= size; i++)
    {
        kernel[size + i] = std::exp(- 4.5 * std::pow((float)i / size, 2)); //< = (3 * sigma) ^ 2 / 2 .
        sum += kernel[size + i];
    }

    // Normalize kernel.
    const float norm = 1 / (2 * sum - kernel[size]); //< kernel[size] was counted twice.
    kernel[size] *= norm;
    for (int i = 1; i <= size; i++)
    {
        kernel[size + i] *= norm;
        kernel[size - i] = kernel[size + i];
    }

    return kernel;
}

} // namespace nx::utils::graphics
