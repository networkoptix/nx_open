#include "warning_style.h"

#include <ui/style/globals.h>

void setWarningStyle(QWidget *widget) {
    QPalette palette = widget->palette();
    palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());
    widget->setPalette(palette);
}

void setWarningStyle(QPalette *palette) {
    palette->setColor(QPalette::WindowText, qnGlobals->errorTextColor());
}
