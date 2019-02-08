#include "palette.h"

#include <QtWidgets/QWidget>
#include <QtWidgets/QGraphicsWidget>

QPalette withColor(const QPalette &palette, QPalette::ColorRole colorRole, const QColor &color) {
    QPalette result = palette;
    result.setColor(colorRole, color);
    return result;
}

QPalette withColor(const QPalette &palette, QPalette::ColorGroup colorGroup, QPalette::ColorRole colorRole, const QColor &color) {
    QPalette result = palette;
    result.setColor(colorGroup, colorRole, color);
    return result;
}

QPalette withBrush(const QPalette &palette, QPalette::ColorRole colorRole, const QBrush &brush) {
    QPalette result = palette;
    result.setBrush(colorRole, brush);
    return result;
}

QPalette withBrush(const QPalette &palette, QPalette::ColorGroup colorGroup, QPalette::ColorRole colorRole, const QBrush &brush) {
    QPalette result = palette;
    result.setBrush(colorGroup, colorRole, brush);
    return result;
}

void setPaletteColor(QWidget *widget, QPalette::ColorRole colorRole, const QColor &color) {
    widget->setPalette(withColor(widget->palette(), colorRole, color));
}

void setPaletteColor(QWidget *widget, QPalette::ColorGroup colorGroup, QPalette::ColorRole colorRole, const QColor &color) {
    widget->setPalette(withColor(widget->palette(), colorGroup, colorRole, color));
}

void setPaletteBrush(QWidget *widget, QPalette::ColorRole colorRole, const QBrush &brush) {
    widget->setPalette(withBrush(widget->palette(), colorRole, brush));
}

void setPaletteBrush(QWidget *widget, QPalette::ColorGroup colorGroup, QPalette::ColorRole colorRole, const QBrush &brush) {
    widget->setPalette(withBrush(widget->palette(), colorGroup, colorRole, brush));
}

void setPaletteColor(QGraphicsWidget *widget, QPalette::ColorRole colorRole, const QColor &color) {
    widget->setPalette(withColor(widget->palette(), colorRole, color));
}

void setPaletteColor(QGraphicsWidget *widget, QPalette::ColorGroup colorGroup, QPalette::ColorRole colorRole, const QColor &color) {
    widget->setPalette(withColor(widget->palette(), colorGroup, colorRole, color));
}

void setPaletteBrush(QGraphicsWidget *widget, QPalette::ColorRole colorRole, const QBrush &brush) {
    widget->setPalette(withBrush(widget->palette(), colorRole, brush));
}

void setPaletteBrush(QGraphicsWidget *widget, QPalette::ColorGroup colorGroup, QPalette::ColorRole colorRole, const QBrush &brush) {
    widget->setPalette(withBrush(widget->palette(), colorGroup, colorRole, brush));
}
