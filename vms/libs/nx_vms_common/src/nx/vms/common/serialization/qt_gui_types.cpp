// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qt_gui_types.h"

#include <QtGui/QColor>
#include <QtGui/QRegion>

#include <nx/fusion/serialization/json_functions.h>

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

void serialize(QnJsonContext* ctx, const QRegion& value, QJsonValue* target)
{
    QJson::serialize(ctx, std::vector(value.begin(), value.end()), target);
}

bool deserialize(QnJsonContext* ctx, const QJsonValue& value, QRegion* target)
{
    if (value.type() == QJsonValue::Null)
    {
        *target = QRegion();
        return true;
    }

    QVarLengthArray<QRect, 32> rects;
    if (!QJson::deserialize(ctx, value, &rects))
        return false;

    target->setRects(rects.data(), rects.size());
    return true;
}
