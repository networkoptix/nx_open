#include "image_processing.h"

#include <cmath>

#include <QtCore/QVarLengthArray>

#include <utils/common/aspect_ratio.h>
#include <utils/common/warnings.h>

#include <nx/utils/math/fuzzy.h>

QImage gaussianBlur(const QImage &src, qreal sigma) {
    QImage result;
    gaussianBlur(src, sigma, &result);
    return result;
}

static void gaussianBlurInternal(const QImage &src, qreal sigma, QImage &dst) {
    int r = qMax(1, static_cast<int>(sigma * 3.0 + 0.5));

    QVarLengthArray<int, 256> kernel(2 * r + 1);
#define K (kernel.data() + r)
    int kernelSum = 0;
    for (int x = -r; x <= r; x++) {
        K[x] = static_cast<int>(256 * std::exp(-(x * x) / (2 * sigma * sigma)));
        kernelSum += K[x];
    }
    /* This gonna never happen, just to make static analyzer feel safe =) */
    if (kernelSum == 0)
        kernelSum = 1;

    int stride = src.bytesPerLine();
    QSize size = src.size();
    QImage tmp(src.size(), QImage::Format_ARGB32);
    dst = QImage(src.size(), QImage::Format_ARGB32);

    unsigned char *srcLine, *dstLine;

    srcLine = (unsigned char *) src.scanLine(0);
    dstLine = (unsigned char *) tmp.scanLine(0);
    for (int y = 0; y < size.height(); y++) {
        for (int x = 0; x < size.width(); x++) {
            int color[4] = {0, 0, 0, 0};
            for (int dx = -r; dx <= r; dx++) {
                int rx = qBound(0, x + dx, size.width() - 1);

                color[0] += K[dx] * srcLine[stride * y + rx * 4 + 0];
                color[1] += K[dx] * srcLine[stride * y + rx * 4 + 1];
                color[2] += K[dx] * srcLine[stride * y + rx * 4 + 2];
                color[3] += K[dx] * srcLine[stride * y + rx * 4 + 3];
            }

            dstLine[stride * y + x * 4 + 0] = color[0] / kernelSum;
            dstLine[stride * y + x * 4 + 1] = color[1] / kernelSum;
            dstLine[stride * y + x * 4 + 2] = color[2] / kernelSum;
            dstLine[stride * y + x * 4 + 3] = color[3] / kernelSum;
        }
    }

    srcLine = (unsigned char *) tmp.scanLine(0);
    dstLine = (unsigned char *) dst.scanLine(0);
    for (int y = 0; y < size.height(); y++) {
        for (int x = 0; x < size.width(); x++) {
            int color[4] = {0, 0, 0, 0};
            for (int dy = -r; dy <= r; dy++) {
                int ry = qBound(0, y + dy, size.height() - 1);

                color[0] += K[dy] * srcLine[stride * ry + x * 4 + 0];
                color[1] += K[dy] * srcLine[stride * ry + x * 4 + 1];
                color[2] += K[dy] * srcLine[stride * ry + x * 4 + 2];
                color[3] += K[dy] * srcLine[stride * ry + x * 4 + 3];
            }

            dstLine[stride * y + x * 4 + 0] = color[0] / kernelSum;
            dstLine[stride * y + x * 4 + 1] = color[1] / kernelSum;
            dstLine[stride * y + x * 4 + 2] = color[2] / kernelSum;
            dstLine[stride * y + x * 4 + 3] = color[3] / kernelSum;
        }
    }

#undef K
}

void gaussianBlur(const QImage &src, qreal sigma, QImage *dst) {
    if(dst == NULL) {
        qnNullWarning(dst);
        return;
    }

    if(sigma <= 0.0) {
        qnWarning("Invalid value of a gaussian filter sigma parameter '%1'.", sigma);
        return;
    }

    gaussianBlurInternal(src.convertToFormat(QImage::Format_ARGB32), sigma, *dst);
}

QImage cropToAspectRatio(const QImage& source, const QnAspectRatio& targetAspectRatio)
{
    return cropToAspectRatio(source, targetAspectRatio.toFloat());
}

QImage cropToAspectRatio(const QImage &source, const qreal targetAspectRatio)
{
    QImage result = source;
    qreal aspectRatio = (qreal)source.width() / (qreal)source.height();
    if (!qFuzzyEquals(aspectRatio, targetAspectRatio))
    {
        if (targetAspectRatio > aspectRatio)
        {
            int targetHeight = qRound((qreal)source.width() / targetAspectRatio);
            int offset = (source.height() - targetHeight) / 2;
            result = source.copy(0, offset, source.width(), targetHeight);
        }
        else
        {
            int targetWidth = qRound((qreal)source.height() * targetAspectRatio);
            int offset = (source.width() - targetWidth / 2);
            result = source.copy(offset, 0, targetWidth, source.height());
        }
    }
    return result;
}
