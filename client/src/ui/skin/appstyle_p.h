#ifndef APPSTYLE_P_H
#define APPSTYLE_P_H

#include <QtGui/QProxyStyle>

#include "ui/proxystyle.h"

class AppStyle : public ProxyStyle
{
    Q_OBJECT

public:
    AppStyle(const QString &baseStyle, QObject *parent = 0);

    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget = 0) const;

    int styleHint(StyleHint hint, const QStyleOption *option = 0, const QWidget *widget = 0, QStyleHintReturn *returnData = 0) const;

    void polish(QApplication *application);
    void unpolish(QApplication *application);
    void polish(QWidget *widget);
    void unpolish(QWidget *widget);
};


class AppProxyStyle : public QProxyStyle
{
    Q_OBJECT

public:
    AppProxyStyle(QStyle *style = 0);

    QRect subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget = 0) const;

    QPixmap standardPixmap(StandardPixmap standardPixmap, const QStyleOption *option = 0, const QWidget *widget = 0) const;

protected Q_SLOTS:
    QIcon standardIconImplementation(StandardPixmap standardIcon, const QStyleOption *option, const QWidget *widget) const;
};

#endif // APPSTYLE_P_H
