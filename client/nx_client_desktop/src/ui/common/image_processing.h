#ifndef QN_IMAGE_PROCESSING_H
#define QN_IMAGE_PROCESSING_H

#include <QtGui/QImage>

void gaussianBlur(const QImage &src, qreal sigma, QImage *dst);

QImage gaussianBlur(const QImage &src, qreal sigma);

QImage cropToAspectRatio(const QImage &source, const qreal targetAspectRatio);

#endif // QN_IMAGE_PROCESSING_H

