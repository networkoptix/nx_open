#ifndef COLOR_H
#define COLOR_H

#include <QtGui/QColor>
#include <QtCore/QVariant>

QColor parseColor(const QString &name, const QColor &defaultValue = QColor());
QColor parseColor(const QVariant &value, const QColor &defaultValue = QColor());

#endif // COLOR_H
