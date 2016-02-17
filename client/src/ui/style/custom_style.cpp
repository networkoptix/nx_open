#include "custom_style.h"

#include <QtWidgets/QPushButton>

#include <ui/style/generic_palette.h>
#include <ui/style/nx_style.h>
#include <ui/common/palette.h>
#include <ui/style/globals.h>

void setWarningStyle(QWidget *widget) {
    setPaletteColor(widget, QPalette::WindowText, qnGlobals->errorTextColor());
    setPaletteColor(widget, QPalette::Text, qnGlobals->errorTextColor());
}

void setWarningStyle(QPalette *palette) {
    palette->setColor(QPalette::WindowText, qnGlobals->errorTextColor());
    palette->setColor(QPalette::Text, qnGlobals->errorTextColor());
}

QString setWarningStyleHtml( const QString &source ) {
    return lit("<font color=\"%1\">%2</font>").arg(qnGlobals->errorTextColor().name(), source);
}

void setAccentStyle(QPushButton *button)
{
    if (QnNxStyle *style = QnNxStyle::instance())
    {
        QColor color = style->mainColor(QnNxStyle::Colors::kBlue);
        setPaletteColor(button, QPalette::Active, QPalette::Button, color);
        setPaletteColor(button, QPalette::Inactive, QPalette::Button, color);
    }
}
