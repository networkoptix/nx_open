#ifndef PROXYSTYLE_P_H
#define PROXYSTYLE_P_H

#include <QtGui/QStyle>
#include <QtGui/QStyleFactory>

class ProxyStyle : public QStyle
{
    Q_OBJECT

public:
    explicit ProxyStyle(QStyle *baseStyle)
        : m_style(baseStyle)
    { m_style->setParent(this); }
    explicit ProxyStyle(const QString &baseStyle)
        : m_style(QStyleFactory::create(baseStyle))
    { m_style->setParent(this); }

    virtual void drawComplexControl(ComplexControl control, const QStyleOptionComplex* option, QPainter* painter, const QWidget* widget = 0) const
    { m_style->drawComplexControl(control, option, painter, widget); }
    virtual void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget = 0) const
    { m_style->drawControl(element, option, painter, widget); }
    virtual void drawItemPixmap(QPainter* painter, const QRect& rectangle, int alignment, const QPixmap& pixmap) const
    { m_style->drawItemPixmap(painter, rectangle, alignment, pixmap); }
    virtual void drawItemText(QPainter* painter, const QRect& rectangle, int alignment, const QPalette& palette, bool enabled, const QString& text, QPalette::ColorRole textRole = QPalette::NoRole) const
    { m_style->drawItemText(painter, rectangle, alignment, palette, enabled, text, textRole); }
    virtual void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget = 0) const
    { m_style->drawPrimitive(element, option, painter, widget); }
    virtual QPixmap generatedIconPixmap(QIcon::Mode iconMode, const QPixmap& pixmap, const QStyleOption* option) const
    { return m_style->generatedIconPixmap(iconMode, pixmap, option); }
    virtual SubControl hitTestComplexControl(ComplexControl control, const QStyleOptionComplex* option, const QPoint& position, const QWidget* widget = 0) const
    { return m_style->hitTestComplexControl(control, option, position, widget); }
    virtual QRect itemPixmapRect(const QRect& rectangle, int alignment, const QPixmap& pixmap) const
    { return m_style->itemPixmapRect(rectangle, alignment, pixmap); }
    virtual QRect itemTextRect(const QFontMetrics& metrics, const QRect& rectangle, int alignment, bool enabled, const QString& text) const
    { return m_style->itemTextRect(metrics, rectangle, alignment, enabled, text); }
    virtual int pixelMetric(PixelMetric metric, const QStyleOption* option = 0, const QWidget* widget = 0) const
    { return m_style->pixelMetric(metric, option, widget); }
    virtual void polish(QWidget* widget)
    { m_style->polish(widget); }
    virtual void polish(QApplication* application)
    { m_style->polish(application); }
    virtual void polish(QPalette& palette)
    { m_style->polish(palette); }
    virtual QSize sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& contentsSize, const QWidget* widget = 0) const
    { return m_style->sizeFromContents(type, option, contentsSize, widget); }
    virtual QPalette standardPalette() const
    { return m_style->standardPalette(); }
    virtual int styleHint(StyleHint hint, const QStyleOption* option = 0, const QWidget* widget = 0, QStyleHintReturn* returnData = 0) const
    { return m_style->styleHint(hint, option, widget, returnData); }
    virtual QRect subControlRect(ComplexControl control, const QStyleOptionComplex* option, SubControl subControl, const QWidget* widget = 0) const
    { return m_style->subControlRect(control, option, subControl, widget); }
    virtual void unpolish(QWidget* widget)
    { m_style->unpolish(widget); }
    virtual void unpolish(QApplication* application)
    { m_style->unpolish(application); }
    virtual QPixmap standardPixmap(StandardPixmap standardPixmap, const QStyleOption* opt = 0, const QWidget* widget = 0) const
    { return m_style->standardPixmap(standardPixmap, opt, widget); }
    virtual QRect subElementRect(SubElement element, const QStyleOption* option, const QWidget* widget = 0) const
    { return m_style->subElementRect(element, option, widget); }
    virtual bool event(QEvent* e)
    { return m_style->event(e); }
    virtual bool eventFilter(QObject* o, QEvent* e)
    { return m_style->eventFilter(o, e); }

protected Q_SLOTS:
    QIcon standardIconImplementation(StandardPixmap standardIcon, const QStyleOption*  option = 0, const QWidget*  widget = 0) const
    { return m_style->standardIcon(standardIcon, option, widget); }

    int layoutSpacingImplementation(QSizePolicy::ControlType control1, QSizePolicy::ControlType control2, Qt::Orientation orientation, const QStyleOption*  option = 0, const QWidget*  widget = 0) const
    { return m_style->layoutSpacing(control1, control2, orientation, option, widget); }

private:
    Q_DISABLE_COPY(ProxyStyle)

    QStyle *const m_style;
};

#endif // PROXYSTYLE_P_H
