#ifndef QN_GRAPHICS_STYLE_H
#define QN_GRAPHICS_STYLE_H

#include <QtCore/QPointer>
#include <QtWidgets/QStyle>

class QStyle;
class QGraphicsWidget;

// TODO: #Elric #2.3 this one is not needed actually.
// We have QStyleOption::styleObject.
// 
// Remove!

class GraphicsStyle {
public:
    GraphicsStyle();
    virtual ~GraphicsStyle();

    QStyle *baseStyle() const;
    void setBaseStyle(QStyle *style);

    const QGraphicsWidget *currentGraphicsWidget() const {
        return m_graphicsWidget;
    }

    const QObject *currentTarget(const QWidget *widget) const;

    void drawItemText(QPainter *painter, const QRect &rectangle, int alignment, const QPalette &palette, bool enabled, const QString &text, QPalette::ColorRole textRole = QPalette::NoRole) const { baseStyle()->drawItemText(painter, rectangle, alignment, palette, enabled, text, textRole); }
    void drawItemPixmap(QPainter *painter, const QRect &rectangle, int alignment, const QPixmap &pixmap) const { baseStyle()->drawItemPixmap(painter, rectangle, alignment, pixmap); }

    QRect itemTextRect(const QFontMetrics &metrics, const QRect &rectangle, int alignment, bool enabled, const QString &text) const { return baseStyle()->itemTextRect(metrics, rectangle, alignment, enabled, text); }
    QRect itemPixmapRect(const QRect &rectangle, int alignment, const QPixmap &pixmap) const { return baseStyle()->itemPixmapRect(rectangle, alignment, pixmap); }

    QPixmap generatedIconPixmap(QIcon::Mode iconMode, const QPixmap &pixmap, const QStyleOption *option) const { return baseStyle()->generatedIconPixmap(iconMode, pixmap, option); }
    QPalette standardPalette() const { return baseStyle()->standardPalette(); }

    void polish(QGraphicsWidget *widget);
    void unpolish(QGraphicsWidget *widget);
    void polish(QPalette &palette);

    QPixmap standardPixmap(QStyle::StandardPixmap standardPixmap, const QStyleOption *option = 0, const QGraphicsWidget *widget = 0) const;
    void drawComplexControl(QStyle::ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QGraphicsWidget *widget = 0) const;
    void drawControl(QStyle::ControlElement element, const QStyleOption *option, QPainter *painter, const QGraphicsWidget *widget = 0) const;
    void drawPrimitive(QStyle::PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QGraphicsWidget *widget = 0) const;
    QStyle::SubControl hitTestComplexControl(QStyle::ComplexControl control, const QStyleOptionComplex *option, const QPoint &position, const QGraphicsWidget *widget = 0) const;
    int pixelMetric(QStyle::PixelMetric metric, const QStyleOption *option = 0, const QGraphicsWidget *widget = 0) const;
    QSize sizeFromContents(QStyle::ContentsType type, const QStyleOption *option, const QSize &contentsSize, const QGraphicsWidget *widget = 0) const;
    int styleHint(QStyle::StyleHint hint, const QStyleOption *option = 0, const QGraphicsWidget *widget = 0, QStyleHintReturn *returnData = 0) const;
    QRect subControlRect(QStyle::ComplexControl control, const QStyleOptionComplex *option, QStyle::SubControl subControl, const QGraphicsWidget *widget = 0) const;
    QRect subElementRect(QStyle::SubElement element, const QStyleOption *option, const QGraphicsWidget *widget = 0) const;

    QStyle::SubControl hitTestComplexControl(QStyle::ComplexControl control, const QStyleOptionComplex *option, const QPointF &position, const QGraphicsWidget *widget = 0) const;

    static qreal sliderPositionFromValue(qint64 min, qint64 max, qint64 logicalValue, qreal span, bool upsideDown, bool bound);
    static qint64 sliderValueFromPosition(qint64 min, qint64 max, qreal pos, qreal span, bool upsideDown, bool bound);

private:
    QPointer<QStyle> m_baseStyle;
    mutable const QGraphicsWidget *m_graphicsWidget;
};


#endif // QN_GRAPHICS_STYLE_H
