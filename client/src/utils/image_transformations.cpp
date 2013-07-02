#include "image_transformations.h"

QImage cropImageToAspectRatio(const QImage &source, const qreal targetAspectRatio) {
    QImage result = source;
    qreal aspectRatio = (qreal)source.width() / (qreal)source.height();
    if (!qFuzzyCompare(aspectRatio, targetAspectRatio)) {
        if (targetAspectRatio > aspectRatio) {
            int targetHeight = qRound((qreal)source.width() / targetAspectRatio);
            int offset = (source.height() - targetHeight) / 2;
            result = source.copy(0, offset, source.width(), targetHeight);
        } else {
            int targetWidth = qRound((qreal)source.height() * targetAspectRatio);
            int offset = (source.width() - targetWidth / 2);
            result = source.copy(offset, 0, targetWidth, source.height());
        }
    }
    return result;
}
