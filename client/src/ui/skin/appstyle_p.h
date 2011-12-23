#ifndef APPSTYLE_P_H
#define APPSTYLE_P_H

#include "ui/proxystyle.h"

class AppStyle : public ProxyStyle
{
    Q_OBJECT

public:
    AppStyle(const QString &baseStyle, QObject *parent = 0) : ProxyStyle(baseStyle, parent) {}

    int styleHint(StyleHint hint, const QStyleOption *option = 0, const QWidget *widget = 0, QStyleHintReturn *returnData = 0) const;

protected Q_SLOTS:
    QIcon standardIconImplementation(StandardPixmap standardIcon, const QStyleOption *option, const QWidget *widget) const;
};

#endif // APPSTYLE_P_H
