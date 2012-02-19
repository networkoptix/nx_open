#ifndef QN_APP_STYLE_H
#define QN_APP_STYLE_H

#include <QProxyStyle>
#include "proxy_style.h"

class AppStyle: public QnProxyStyle { 
    Q_OBJECT;

    typedef QnProxyStyle base_type;

public:
    AppStyle(const QString &baseStyle, QObject *parent = 0);

    virtual void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget = 0) const override;
    virtual void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = 0) const override;
    virtual void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = 0) const override;

    virtual int styleHint(StyleHint hint, const QStyleOption *option = 0, const QWidget *widget = 0, QStyleHintReturn *returnData = 0) const override;

    virtual void polish(QApplication *application) override;
    virtual void unpolish(QApplication *application) override;
    virtual void polish(QWidget *widget) override;
    virtual void unpolish(QWidget *widget) override;

protected:
    bool drawMenuItemControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    bool drawSliderComplexControl(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const;
    bool drawTabClosePrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
};


class AppProxyStyle : public QProxyStyle
{
    Q_OBJECT

public:
    AppProxyStyle(QStyle *style = 0);

    virtual QRect subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget = 0) const override;

    virtual QPixmap standardPixmap(StandardPixmap standardPixmap, const QStyleOption *option = 0, const QWidget *widget = 0) const override;

protected Q_SLOTS:
    QIcon standardIconImplementation(StandardPixmap standardIcon, const QStyleOption *option, const QWidget *widget) const;
};

namespace {
    const char *hideCheckBoxInMenuPropertyName = "_qn_hideCheckBoxInMenu";
}


#endif // QN_APP_STYLE_H
