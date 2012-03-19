#ifndef QN_PROXY_STYLE_H
#define QN_PROXY_STYLE_H

#include <QtCore/QPointer>

#include <QtGui/QStyle>

/**
 * This class implements a proxy style that doesn't affect its base style in any way,
 * unlike the <tt>QProxyStyle</tt>. 
 * 
 * In case of <tt>QProxyStyle</tt>, only one proxy style can be constructed for
 * any given style instance. This class doesn't have this limitation.
 * 
 * The downside is that the style that is being proxied cannot call back into the proxy.
 */
class QnProxyStyle : public QStyle
{
    Q_OBJECT

public:
    explicit QnProxyStyle(QStyle *baseStyle, QObject *parent = NULL);
    explicit QnProxyStyle(const QString &baseStyle, QObject *parent = NULL);

    QStyle *baseStyle() const;
    void setBaseStyle(QStyle *style);

    virtual void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = 0) const
    { baseStyle()->drawPrimitive(element, option, painter, widget); }
    virtual void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = 0) const
    { baseStyle()->drawControl(element, option, painter, widget); }
    virtual void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget = 0) const
    { baseStyle()->drawComplexControl(control, option, painter, widget); }
    virtual void drawItemText(QPainter *painter, const QRect &rectangle, int alignment, const QPalette &palette, bool enabled, const QString &text, QPalette::ColorRole textRole = QPalette::NoRole) const
    { baseStyle()->drawItemText(painter, rectangle, alignment, palette, enabled, text, textRole); }
    virtual void drawItemPixmap(QPainter *painter, const QRect &rectangle, int alignment, const QPixmap &pixmap) const
    { baseStyle()->drawItemPixmap(painter, rectangle, alignment, pixmap); }

    virtual QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contentsSize, const QWidget *widget = 0) const
    { return baseStyle()->sizeFromContents(type, option, contentsSize, widget); }

    virtual QRect subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget = 0) const
    { return baseStyle()->subElementRect(element, option, widget); }
    virtual QRect subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget = 0) const
    { return baseStyle()->subControlRect(control, option, subControl, widget); }
    virtual QRect itemTextRect(const QFontMetrics &metrics, const QRect &rectangle, int alignment, bool enabled, const QString &text) const
    { return baseStyle()->itemTextRect(metrics, rectangle, alignment, enabled, text); }
    virtual QRect itemPixmapRect(const QRect &rectangle, int alignment, const QPixmap &pixmap) const
    { return baseStyle()->itemPixmapRect(rectangle, alignment, pixmap); }

    virtual SubControl hitTestComplexControl(ComplexControl control, const QStyleOptionComplex *option, const QPoint &position, const QWidget *widget = 0) const
    { return baseStyle()->hitTestComplexControl(control, option, position, widget); }
    virtual int styleHint(StyleHint hint, const QStyleOption *option = 0, const QWidget *widget = 0, QStyleHintReturn *returnData = 0) const
    { return baseStyle()->styleHint(hint, option, widget, returnData); }
    virtual int pixelMetric(PixelMetric metric, const QStyleOption *option = 0, const QWidget *widget = 0) const
    { return baseStyle()->pixelMetric(metric, option, widget); }

    virtual QPixmap standardPixmap(StandardPixmap standardPixmap, const QStyleOption *option = 0, const QWidget *widget = 0) const
    { return baseStyle()->standardPixmap(standardPixmap, option, widget); }
    virtual QPixmap generatedIconPixmap(QIcon::Mode iconMode, const QPixmap &pixmap, const QStyleOption *option) const
    { return baseStyle()->generatedIconPixmap(iconMode, pixmap, option); }
    virtual QPalette standardPalette() const
    { return baseStyle()->standardPalette(); }

    virtual void polish(QApplication *application)
    { baseStyle()->polish(application); }
    virtual void unpolish(QApplication *application)
    { baseStyle()->unpolish(application); }
    virtual void polish(QWidget *widget)
    { baseStyle()->polish(widget); }
    virtual void unpolish(QWidget *widget)
    { baseStyle()->unpolish(widget); }
    virtual void polish(QPalette &palette)
    { baseStyle()->polish(palette); }

protected:
    virtual bool event(QEvent *e)
    { return baseStyle()->event(e); }
    virtual bool eventFilter(QObject *o, QEvent *e)
    { return baseStyle()->eventFilter(o, e); }

protected slots:
    QIcon standardIconImplementation(StandardPixmap standardIcon, const QStyleOption *option = 0, const QWidget *widget = 0) const
    { return baseStyle()->standardIcon(standardIcon, option, widget); }

    int layoutSpacingImplementation(QSizePolicy::ControlType control1, QSizePolicy::ControlType control2,
                                    Qt::Orientation orientation, const QStyleOption *option = 0, const QWidget *widget = 0) const
    { return baseStyle()->layoutSpacing(control1, control2, orientation, option, widget); }

private:
    Q_DISABLE_COPY(QnProxyStyle)

    QWeakPointer<QStyle> m_style;
};

#endif // QN_PROXY_STYLE_H
