#pragma once

#include <vector>

class QImage;

/** QWidgets-free Blur Filter. If you can use QWidgets, use QPixmapBlurFilter instead. */

namespace nx::utils::graphics
{

class BlurFilter
{
public:
    BlurFilter(int blurWidth = 6);

    /**  Blurs image. Image edges (of blurWidth) are not processed. */
    void filterImage(QImage& image);
    /**  Blurs only Alpha channel. Removes other channels. Faster. */
    void filterImageAlpha(QImage& image);


private:
    const int m_kernelWidth;
    const std::vector<float> m_kernel;

    void filterImage(QImage& image, void (BlurFilter::* filter1D)(QImage image, QImage& blurred) const);
    //void filterImage(QImage& image,  std::function<void(QImage image, QImage& blurred)> filter1D);

    void filter1DARGB(QImage image, QImage& blurred) const;
    void filter1DAlpha(QImage image, QImage& blurred) const;

    std::vector<float> createKernel(int size) const;

};

} // namespace nx::utils::graphics