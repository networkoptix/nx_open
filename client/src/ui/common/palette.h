#ifndef QN_PALETTE_H
#define QN_PALETTE_H

#include <QtGui/QPalette>

class QWidget;
class QGraphicsWidget;

QPalette withColor(const QPalette &palette, QPalette::ColorRole colorRole, const QColor &color);
QPalette withColor(const QPalette &palette, QPalette::ColorGroup colorGroup, QPalette::ColorRole colorRole, const QColor &color);
QPalette withBrush(const QPalette &palette, QPalette::ColorRole colorRole, const QBrush &brush);
QPalette withBrush(const QPalette &palette, QPalette::ColorGroup colorGroup, QPalette::ColorRole colorRole, const QBrush &brush);

void setPaletteColor(QWidget *widget, QPalette::ColorRole colorRole, const QColor &color);
void setPaletteColor(QWidget *widget, QPalette::ColorGroup colorGroup, QPalette::ColorRole colorRole, const QColor &color);
void setPaletteBrush(QWidget *widget, QPalette::ColorRole colorRole, const QBrush &brush);
void setPaletteBrush(QWidget *widget, QPalette::ColorGroup colorGroup, QPalette::ColorRole colorRole, const QBrush &brush);

void setPaletteColor(QGraphicsWidget *widget, QPalette::ColorRole colorRole, const QColor &color);
void setPaletteColor(QGraphicsWidget *widget, QPalette::ColorGroup colorGroup, QPalette::ColorRole colorRole, const QColor &color);
void setPaletteBrush(QGraphicsWidget *widget, QPalette::ColorRole colorRole, const QBrush &brush);
void setPaletteBrush(QGraphicsWidget *widget, QPalette::ColorGroup colorGroup, QPalette::ColorRole colorRole, const QBrush &brush);


#endif // QN_PALETTE_H
