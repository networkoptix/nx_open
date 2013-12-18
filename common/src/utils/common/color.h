#ifndef QN_COLOR_H
#define QN_COLOR_H

#include <QtGui/QColor>
#include <QtCore/QVariant>

QColor parseColor(const QString &name, const QColor &defaultValue = QColor());
QColor parseColor(const QVariant &value, const QColor &defaultValue = QColor());

#endif // QN_COLOR_H
