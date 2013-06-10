#include "image_correction.h"
#include "utils/math/math.h"

static const int MIN_GAMMA_RANGE = 6;
static const float NORM_RANGE_START = 0.0; //16.0
static const float NORM_RANGE_RANGE = 256.0 - NORM_RANGE_START*2.0;

bool ImageCorrectionParams::operator== (const ImageCorrectionParams& other) const
{
    if (!qFuzzyCompare(blackLevel, other.blackLevel))
        return false;

    if (!qFuzzyCompare(whiteLevel, other.whiteLevel))
        return false;

    if (!qFuzzyCompare(gamma, other.gamma))
        return false;

    return enabled == other.enabled;
}

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
    return qBound(0.5, (qreal) log(recValue) / log(curValue), (qreal) 1.5);
}

void ImageCorrectionResult::analizeImage(const quint8* yPlane, int width, int height, int stride, 
                                         const ImageCorrectionParams& data, const QRectF& srcRect)
{
    Q_ASSERT(width % 4 == 0 && stride % 4 == 0);
    memset(hystogram, 0, sizeof(hystogram));


    if (data.blackLevel == 0 && data.whiteLevel == 0 && data.gamma == 1.0 || yPlane == 0)
    {
        clear();
        return;
    }

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
    int leftSum = 0;
    for (; leftPos < 256; ++leftPos) {
        if (leftSum+hystogram[leftPos] >= leftThreshold)
            break;
        leftSum += hystogram[leftPos];
    }

    int rightPos = 255;
    int rightSum = 0;
    for (; rightPos >= leftPos; --rightPos) {
        if (rightSum+hystogram[rightPos] >= rightThreshold)
            break;
        rightSum += hystogram[rightPos];
    }

    if (rightPos - leftPos < MIN_GAMMA_RANGE) {
        clear();
    }
    else {
        aCoeff = NORM_RANGE_RANGE / float(rightPos-leftPos+1);
        bCoeff = -float(leftPos) / 256.0 + NORM_RANGE_START/256.0;
        gamma = data.gamma;
        if (gamma == 0)
            gamma = calcGamma(leftPos, rightPos, pixels - leftSum - rightSum); // auto gamma
    }
}

void ImageCorrectionResult::clear()
{
    aCoeff = 1.0;
    bCoeff = 0.0;
    gamma = 1.0;
}
