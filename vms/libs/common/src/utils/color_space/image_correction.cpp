#include "image_correction.h"

#include <memory>

#include <utils/math/math.h>

#include <nx/vms/api/data/image_correction_data.h>

#include <nx/fusion/model_functions.h>

static const int MIN_GAMMA_RANGE = 6;
static const float NORM_RANGE_START = 0.0; //16.0
static const float NORM_RANGE_RANGE = 256.0 - NORM_RANGE_START*2.0;

float ImageCorrectionResult::calcGamma(int leftPos, int rightPos, int pixels) const
{
    // 1. calc hystogram middle point
    int sum = 0;
    int median = leftPos;
    for (; median <= rightPos && sum < pixels/2; ++median)
        sum += hystogram[median];


    // 2. calc gamma
    qreal curValue = (median - leftPos) / qreal(rightPos-leftPos+1);
    qreal recValue = 0.5;
    return qBound<double>(0.5, (qreal) log(recValue) / log(curValue), (qreal) 1.5);
}

void ImageCorrectionResult::analyseImage(
    const quint8* yPlane,
    int width,
    int height,
    int stride,
    const nx::vms::api::ImageCorrectionData& data,
    const QRectF& srcRect)
{
    if (!data.enabled || yPlane == 0)
    {
        filled = false;
        return;
    }

    NX_ASSERT(stride % 4 == 0);

    int left = qPower2Floor(srcRect.left()*width, 4);
    int right = qPower2Floor(srcRect.right()*width, 4);
    int top = srcRect.top()*height;
    int bottom = srcRect.bottom()*height;

    int xSteps = (right-left) / 4;

    // prepare hystogram
    memset(hystogram, 0, sizeof(hystogram));
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
    int leftSum = 0;
    for (; leftPos < 256-MIN_GAMMA_RANGE; ++leftPos) {
        if (leftSum+hystogram[leftPos] >= leftThreshold)
            break;
        leftSum += hystogram[leftPos];
    }

    int rightPos = 255;
    int rightSum = 0;
    for (; rightPos >= leftPos+MIN_GAMMA_RANGE; --rightPos) {
        if (rightSum+hystogram[rightPos] >= rightThreshold)
            break;
        rightSum += hystogram[rightPos];
    }

    aCoeff = NORM_RANGE_RANGE / float(rightPos-leftPos+1);
    bCoeff = -float(leftPos) / 256.0 + NORM_RANGE_START/256.0;
    gamma = data.gamma;
    if (gamma == 0)
        gamma = calcGamma(leftPos, rightPos, pixels - leftSum - rightSum); // auto gamma
    filled = true;
}
