#ifndef COLOR_H
#define COLOR_H

#include <QColor>
#include <QVariant>

QColor parseColor(const QString &name, const QColor &defaultValue = QColor());
QColor parseColor(const QVariant &value, const QColor &defaultValue = QColor());

#endif // COLOR_H
