#ifndef QN_COLOR_TRANSFORMATIONS_H
#define QN_COLOR_TRANSFORMATIONS_H

#include <QtGui/QColor>

QColor shiftColor(const QColor &color, int deltaR, int deltaG, int deltaB, int deltaA = 0);
QColor addColor(const QColor &color, const QColor &shift);
QColor subColor(const QColor &color, const QColor &shift);

QColor toGrayscale(const QColor &color);
QColor toTransparent(const QColor &color);
QColor toTransparent(const QColor &color, qreal opacity);

#endif // QN_COLOR_TRANSFORMATIONS_H
