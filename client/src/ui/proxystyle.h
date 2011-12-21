#ifndef PROXYSTYLE_H
#define PROXYSTYLE_H

#include <QtCore/QPointer>

#include <QtGui/QStyle>

class ProxyStyle : public QStyle
{
    Q_OBJECT

public:
    explicit ProxyStyle(QStyle *baseStyle);
    explicit ProxyStyle(const QString &baseStyle, QObject *parent = 0);

    QStyle *baseStyle() const;
    void setBaseStyle(QStyle *style);

    virtual void drawComplexControl(ComplexControl control, const QStyleOptionComplex* option, QPainter* painter, const QWidget* widget = 0) const
    { baseStyle()->drawComplexControl(control, option, painter, widget); }
    virtual void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget = 0) const
    { baseStyle()->drawControl(element, option, painter, widget); }
    virtual void drawItemPixmap(QPainter* painter, const QRect& rectangle, int alignment, const QPixmap& pixmap) const
    { baseStyle()->drawItemPixmap(painter, rectangle, alignment, pixmap); }
    virtual void drawItemText(QPainter* painter, const QRect& rectangle, int alignment, const QPalette& palette, bool enabled, const QString& text, QPalette::ColorRole textRole = QPalette::NoRole) const
    { baseStyle()->drawItemText(painter, rectangle, alignment, palette, enabled, text, textRole); }
    virtual void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget = 0) const
    { baseStyle()->drawPrimitive(element, option, painter, widget); }
    virtual QPixmap generatedIconPixmap(QIcon::Mode iconMode, const QPixmap& pixmap, const QStyleOption* option) const
    { return baseStyle()->generatedIconPixmap(iconMode, pixmap, option); }
    virtual SubControl hitTestComplexControl(ComplexControl control, const QStyleOptionComplex* option, const QPoint& position, const QWidget* widget = 0) const
    { return baseStyle()->hitTestComplexControl(control, option, position, widget); }
    virtual QRect itemPixmapRect(const QRect& rectangle, int alignment, const QPixmap& pixmap) const
    { return baseStyle()->itemPixmapRect(rectangle, alignment, pixmap); }
    virtual QRect itemTextRect(const QFontMetrics& metrics, const QRect& rectangle, int alignment, bool enabled, const QString& text) const
    { return baseStyle()->itemTextRect(metrics, rectangle, alignment, enabled, text); }
    virtual int pixelMetric(PixelMetric metric, const QStyleOption* option = 0, const QWidget* widget = 0) const
    { return baseStyle()->pixelMetric(metric, option, widget); }
    virtual void polish(QWidget* widget)
    { baseStyle()->polish(widget); }
    virtual void polish(QApplication* application)
    { baseStyle()->polish(application); }
    virtual void polish(QPalette& palette)
    { baseStyle()->polish(palette); }
    virtual QSize sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& contentsSize, const QWidget* widget = 0) const
    { return baseStyle()->sizeFromContents(type, option, contentsSize, widget); }
    virtual QPalette standardPalette() const
    { return baseStyle()->standardPalette(); }
    virtual int styleHint(StyleHint hint, const QStyleOption* option = 0, const QWidget* widget = 0, QStyleHintReturn* returnData = 0) const
    { return baseStyle()->styleHint(hint, option, widget, returnData); }
    virtual QRect subControlRect(ComplexControl control, const QStyleOptionComplex* option, SubControl subControl, const QWidget* widget = 0) const
    { return baseStyle()->subControlRect(control, option, subControl, widget); }
    virtual void unpolish(QWidget* widget)
    { baseStyle()->unpolish(widget); }
    virtual void unpolish(QApplication* application)
    { baseStyle()->unpolish(application); }
    virtual QPixmap standardPixmap(StandardPixmap standardPixmap, const QStyleOption* opt = 0, const QWidget* widget = 0) const
    { return baseStyle()->standardPixmap(standardPixmap, opt, widget); }
    virtual QRect subElementRect(SubElement element, const QStyleOption* option, const QWidget* widget = 0) const
    { return baseStyle()->subElementRect(element, option, widget); }

    virtual bool event(QEvent* e)
    { return baseStyle()->event(e); }
    virtual bool eventFilter(QObject* o, QEvent* e)
    { return baseStyle()->eventFilter(o, e); }

protected Q_SLOTS:
    QIcon standardIconImplementation(StandardPixmap standardIcon, const QStyleOption*  option = 0, const QWidget*  widget = 0) const
    { return baseStyle()->standardIcon(standardIcon, option, widget); }

    int layoutSpacingImplementation(QSizePolicy::ControlType control1, QSizePolicy::ControlType control2, Qt::Orientation orientation, const QStyleOption*  option = 0, const QWidget*  widget = 0) const
    { return baseStyle()->layoutSpacing(control1, control2, orientation, option, widget); }

private:
    Q_DISABLE_COPY(ProxyStyle)

    QPointer<QStyle> m_style;
};

#endif // PROXYSTYLE_H
