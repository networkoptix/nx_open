// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qt_gui_types.h"

#include <QtGui/QColor>

std::string toString(const QColor& value)
{
    const auto format = (value.alphaF() < 1.0) ? QColor::HexArgb : QColor::HexRgb;
    return value.name(format).toStdString();
}

bool fromString(const std::string_view& str, QColor* value)
{
    QColor result(QLatin1String(str.data(), str.length()));
    if (!result.isValid())
        return false;

    *value = result;
    return true;
}
