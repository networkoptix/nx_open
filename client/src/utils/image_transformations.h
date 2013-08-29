#ifndef IMAGE_TRANSFORMATIONS_H
#define IMAGE_TRANSFORMATIONS_H

#include <QtGui/QImage>

QImage cropImageToAspectRatio(const QImage &source, const qreal targetAspectRatio);

#endif // IMAGE_TRANSFORMATIONS_H
