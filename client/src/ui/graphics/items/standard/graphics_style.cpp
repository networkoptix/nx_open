#include "graphics_style.h"
#include <cassert>
#include <QtGui/QApplication>
#include <utils/common/scoped_value_rollback.h>

GraphicsStyle::GraphicsStyle():
    m_graphicsWidget(NULL)
{}

GraphicsStyle::~GraphicsStyle() {
    return;
}

QStyle *GraphicsStyle::baseStyle() const {
    if (!m_baseStyle)
        const_cast<GraphicsStyle *>(this)->setBaseStyle(0); /* Will reinit base style with QApplication's style. */
    return m_baseStyle.data();
}

void GraphicsStyle::setBaseStyle(QStyle *style) {
    if(style) {
        m_baseStyle = style;
    } else {
        m_baseStyle = QApplication::style();
    }
}

const QObject *GraphicsStyle::currentTarget(const QWidget *widget) const {
    if(widget) {
        return widget;
    } else {
        return m_graphicsWidget;
    }
}

QPixmap GraphicsStyle::standardPixmap(QStyle::StandardPixmap standardPixmap, const QStyleOption *option, const QGraphicsWidget *widget) const {
    QnScopedValueRollback<const QGraphicsWidget *> rollback(&m_graphicsWidget, widget);

    return baseStyle()->standardPixmap(standardPixmap, option, NULL);
}

void GraphicsStyle::drawComplexControl(QStyle::ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QGraphicsWidget *widget) const {
    QnScopedValueRollback<const QGraphicsWidget *> rollback(&m_graphicsWidget, widget);

    baseStyle()->drawComplexControl(control, option, painter, NULL);
}

void GraphicsStyle::drawControl(QStyle::ControlElement element, const QStyleOption *option, QPainter *painter, const QGraphicsWidget *widget) const {
    QnScopedValueRollback<const QGraphicsWidget *> rollback(&m_graphicsWidget, widget);

    baseStyle()->drawControl(element, option, painter, NULL);
}

void GraphicsStyle::drawPrimitive(QStyle::PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QGraphicsWidget *widget) const {
    QnScopedValueRollback<const QGraphicsWidget *> rollback(&m_graphicsWidget, widget);

    baseStyle()->drawPrimitive(element, option, painter, NULL);
}

QStyle::SubControl GraphicsStyle::hitTestComplexControl(QStyle::ComplexControl control, const QStyleOptionComplex *option, const QPoint &position, const QGraphicsWidget *widget) const {
    QnScopedValueRollback<const QGraphicsWidget *> rollback(&m_graphicsWidget, widget);

    return baseStyle()->hitTestComplexControl(control, option, position, NULL);
}

int GraphicsStyle::pixelMetric(QStyle::PixelMetric metric, const QStyleOption *option, const QGraphicsWidget *widget) const {
    QnScopedValueRollback<const QGraphicsWidget *> rollback(&m_graphicsWidget, widget);

    return baseStyle()->pixelMetric(metric, option, NULL);
}

QSize GraphicsStyle::sizeFromContents(QStyle::ContentsType type, const QStyleOption *option, const QSize &contentsSize, const QGraphicsWidget *widget) const {
    QnScopedValueRollback<const QGraphicsWidget *> rollback(&m_graphicsWidget, widget);

    return baseStyle()->sizeFromContents(type, option, contentsSize, NULL);
}

int	GraphicsStyle::styleHint(QStyle::StyleHint hint, const QStyleOption *option, const QGraphicsWidget *widget, QStyleHintReturn *returnData) const {
    QnScopedValueRollback<const QGraphicsWidget *> rollback(&m_graphicsWidget, widget);

    return baseStyle()->styleHint(hint, option, NULL, returnData);
}

QRect GraphicsStyle::subControlRect(QStyle::ComplexControl control, const QStyleOptionComplex *option, QStyle::SubControl subControl, const QGraphicsWidget *widget) const {
    QnScopedValueRollback<const QGraphicsWidget *> rollback(&m_graphicsWidget, widget);

    return baseStyle()->subControlRect(control, option, subControl, NULL);
}

QRect GraphicsStyle::subElementRect(QStyle::SubElement element, const QStyleOption *option, const QGraphicsWidget *widget) const {
    QnScopedValueRollback<const QGraphicsWidget *> rollback(&m_graphicsWidget, widget);

    return baseStyle()->subElementRect(element, option, NULL);
}