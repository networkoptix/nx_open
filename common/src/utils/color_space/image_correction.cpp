#include "image_correction.h"
#include "utils/math/math.h"

void ImageCorrectionResult::processImage( quint8* yPlane, int width, int height, int stride, 
                                         const ImageCorrectionParams& data, const QRectF& srcRect)
{
    static const int MIN_GAMMA_RANGE = 6;
    static const float NORM_RANGE_RANGE = 256.0; // - 16.0 - 16.0;
    static const float NORM_RANGE_START = 0.0; //16.0 / 256.0;

    Q_ASSERT(width % 4 == 0 && stride % 4 == 0);
    memset(hystogram, 0, sizeof(hystogram));

    int left = qPower2Floor(srcRect.left()*width, 4);
    int right = qPower2Floor(srcRect.right()*width, 4);
    int top = srcRect.top()*height;
    int bottom = srcRect.bottom()*height;

    int xSteps = (right-left) / 4;

    // prepare hystogram
    for (int y = top; y < bottom; ++y)
    {
        quint32* line = (quint32*) (yPlane + stride * y + left);
        quint32* lineEnd = line + xSteps;
        for (;line < lineEnd; ++line)
        {
            quint32 value = *line;

            hystogram[(quint8) value]++;
            value >>= 8;

            hystogram[(quint8) value]++;
            value >>= 8;

            hystogram[(quint8) value]++;
            value >>= 8;

            hystogram[(quint8) value]++;
        }
    }

    // get hystogram range
    int pixels = (right-left) * (bottom-top);
    int leftThreshold = data.blackLevel * pixels + 0.5;
    int rightThreshold = data.whiteLevel * pixels + 0.5;

    int leftPos = 0;
    int sum = 0;
    for (; leftPos < 256 && sum < leftThreshold; ++leftPos)
        sum += hystogram[leftPos];

    int rightPos = 255;
    sum = 0;
    for (; rightPos >= leftPos && sum < rightThreshold; --rightPos)
        sum += hystogram[rightPos];

    if (rightPos - leftPos < MIN_GAMMA_RANGE) {
        reset();
    }
    else {
        aCoeff = NORM_RANGE_RANGE / float(rightPos-leftPos+1);
        bCoeff = -float(leftPos) / 256.0 + NORM_RANGE_START;
        gamma = data.gamma;
    }
}

void ImageCorrectionResult::reset()
{
    aCoeff = 1.0;
    bCoeff = 0.0;
    gamma = 1.0;
}
