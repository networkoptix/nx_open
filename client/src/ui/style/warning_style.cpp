#include "warning_style.h"

#include <ui/common/palette.h>
#include <ui/style/globals.h>

void setWarningStyle(QWidget *widget) {
    setPaletteColor(widget, QPalette::WindowText, qnGlobals->errorTextColor());
}

void setWarningStyle(QPalette *palette) {
    palette->setColor(QPalette::WindowText, qnGlobals->errorTextColor());
}
