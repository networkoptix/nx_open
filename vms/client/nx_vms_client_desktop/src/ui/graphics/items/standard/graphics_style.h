#ifndef QN_GRAPHICS_STYLE_H
#define QN_GRAPHICS_STYLE_H

#include <QtCore/QtGlobal>

class QGraphicsWidget;

class GraphicsStyle {
public:
    GraphicsStyle();
    virtual ~GraphicsStyle();

    virtual void polish(QGraphicsWidget *widget);
    virtual void unpolish(QGraphicsWidget *widget);

    static qreal sliderPositionFromValue(qint64 min, qint64 max, qint64 logicalValue, qreal span, bool upsideDown, bool bound);
    static qint64 sliderValueFromPosition(qint64 min, qint64 max, qreal pos, qreal span, bool upsideDown, bool bound);
};

#endif // QN_GRAPHICS_STYLE_H
