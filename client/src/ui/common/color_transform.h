#ifndef __QNCOLORS_H_
#define __QNCOLORS_H_

#include <QColor>

QColor shiftColor(const QColor& color, int deltaR, int deltaG, int deltaB, int alpha = 0);
QColor addColor(const QColor& color, const QColor& shift);
QColor subColor(const QColor& color, const QColor& shift);
void toGrayColor(QColor& color);

#endif // __QNCOLORS_H_
