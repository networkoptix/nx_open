// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lexical_functions.h"

#include <QtCore/QStringBuilder>
#include <QtCore/QRegularExpression>
#include <QtCore/QRect>
#include <QtCore/QRectF>

#include <nx/utils/switch.h>

void serialize(const bool &value, QString *target)
{
    *target = value ? QLatin1String("true") : QLatin1String("false");
}

bool deserialize(const QString &value, bool *target)
{
    using qunicodechar = char16_t;

    /* We also support upper/lower case combinations and "0" & "1"
     * during deserialization. */
    if (value.isEmpty())
        return false;

    switch (value[0].unicode())
    {
        case L'0':
            if (value.size() != 1)
            {
                return false;
            }
            else
            {
                *target = false;
                return true;
            }
        case L'1':
            if (value.size() != 1)
            {
                return false;
            }
            else
            {
                *target = true;
                return true;
            }
        case L't':
        case L'T':
            if (value.size() != 4)
            {
                return false;
            }
            else
            {
                const qunicodechar *data = reinterpret_cast<const qunicodechar *>(value.data());
                const qunicodechar *rueData = QT_UNICODE_LITERAL("rue");
                if (memcmp(data + 1, rueData, sizeof(qunicodechar) * 3) == 0)
                {
                    *target = true;
                    return true;
                }
                else if ((data[1] == L'r' || data[1] == L'R') && (data[2] == L'u' || data[2] == L'U') && (data[3] == L'e' || data[3] == L'E'))
                {
                    *target = true;
                    return true;
                }
                else
                {
                    return false;
                }
            }
        case L'f':
        case L'F':
            if (value.size() != 5)
            {
                return false;
            }
            else
            {
                const qunicodechar *data = reinterpret_cast<const qunicodechar *>(value.data());
                const qunicodechar *alseData = QT_UNICODE_LITERAL("alse");
                if (memcmp(data + 1, alseData, sizeof(qunicodechar) * 4) == 0)
                {
                    *target = false;
                    return true;
                }
                else if ((data[1] == L'a' || data[1] == L'A') && (data[2] == L'l' || data[2] == L'L') && (data[3] == L's' || data[3] == L'S') && (data[4] == L'e' || data[4] == L'E'))
                {
                    *target = false;
                    return true;
                }
                else
                {
                    return false;
                }
            }
        default:
            return false;
    }
}

void serialize(const QnUuid &value, QString *target)
{
    *target = value.toString();
}

bool deserialize(const QString &value, QnUuid *target)
{
    QnUuid result = QnUuid::fromStringSafe(value);
    if (result.isNull()
        && !value.isEmpty()
        && value != QLatin1String("00000000-0000-0000-0000-000000000000")
        && value != QLatin1String("{00000000-0000-0000-0000-000000000000}"))
    {
        return false;
    }

    *target = result;
    return true;
}

void serialize(const QUrl &value, QString *target)
{
    *target = value.toString();
}

bool deserialize(const QString &value, QUrl *target)
{
    *target = QUrl(value);
    return true;
}

void serialize(const QColor &value, QString *target)
{
    *target = value.name();
}

bool deserialize(const QString &value, QColor *target)
{
    QString trimmedName = value.trimmed();
    if (trimmedName.startsWith(QLatin1String("QColor")))
    {
        /* QColor(R, G, B, A) format. */
        trimmedName = trimmedName.mid(trimmedName.indexOf('(') + 1);
        trimmedName = trimmedName.left(trimmedName.lastIndexOf(')'));

        QStringList args = trimmedName.split(',');
        if (args.size() < 3 || args.size() > 4)
            return false;

        QList<int> colors;
        for (const QString &arg : args)
        {
            bool ok = false;
            int color = arg.toInt(&ok);
            if (!ok)
                return false;
            colors.push_back(color);
        }

        QColor result(colors[0], colors[1], colors[2]);
        if (colors.size() == 4)
            result.setAlpha(colors[3]);

        *target = result;
        return true;
    }
    else if (trimmedName.startsWith('#') && trimmedName.size() == 9)
    {
    /* #RRGGBBAA format. */
        QColor result(trimmedName.left(7));
        if (!result.isValid())
            return false;

        bool success = false;
        int alpha = trimmedName.right(2).toInt(&success, 16);
        if (!success)
            return false;

        result.setAlpha(alpha);
        *target = result;
        return true;
    }
    else
    {
     /* Named format. */
        QColor result(trimmedName);
        if (result.isValid())
        {
            *target = result;
            return true;
        }
        else
        {
            return false;
        }
    }
}

bool serialize(const QBitArray& value, QString* target)
{
    if (value.isEmpty())
    {
        *target = QByteArray();
        return true;
    }

    const int size = value.size();

    QByteArray byteArray((size + 7) / 8 + 1, '\0');

    for (int i = 0; i < size; ++i)
    {
        char& byte = byteArray[i / 8];
        byte = static_cast<char>(byte | (value.testBit(i) ? 1 : 0) << (i % 8));
    }

    const int lastByteSize = size - (byteArray.size() - 2) * 8;
    byteArray[byteArray.size() - 1] = static_cast<char>(lastByteSize);

    serialize(byteArray, target);
    return true;
}

bool deserialize(const QString& value, QBitArray* target)
{
    QByteArray byteArray;

    if (!deserialize(value, &byteArray))
        return false;

    if (byteArray.isEmpty())
    {
        *target = QBitArray();
        return true;
    }

    const int lastByteSize = byteArray[byteArray.size() - 1];

    if (byteArray.size() == 1)
    {
        if (lastByteSize != 0)
            return false;

        *target = QBitArray();
        return true;
    }

    const int size = (byteArray.size() - 2) * 8 + lastByteSize;
    target->resize(size);

    for (int i = 0; i < size; ++i)
        target->setBit(i, byteArray[i / 8] & (1 << (i % 8)));

    return true;
}

bool serialize(const QRect& value, QString* target)
{
    *target =
        QString::number(value.x())
        % ","
        % QString::number(value.y())
        % ","
        % QString::number(value.width())
        % "x"
        % QString::number(value.height());
    return true;
}

bool deserialize(const QString& value, QRect* target)
{
    const auto params = QStringView(value).split(',');
    if (params.size() != 3)
        return false;
    const auto dimensions = params[2].split('x');
    if (dimensions.size() != 2)
        return false;

    bool success = false;
    target->setX(params[0].toInt(&success));
    if (!success)
        return false;

    target->setY(params[1].toInt(&success));
    if (!success)
        return false;

    target->setWidth(dimensions[0].toInt(&success));
    if (!success)
        return false;

    target->setHeight(dimensions[1].toInt(&success));
    return success;
}

bool serialize(const QRectF& value, QString* target)
{
    *target =
        QString::number(value.x())
        % ","
        % QString::number(value.y())
        % ","
        % QString::number(value.width())
        % "x"
        % QString::number(value.height());
    return true;
}

bool deserialize(const QString& value, QRectF* target)
{
    const auto params = QStringView(value).split(',');
    if (params.size() != 3)
        return false;
    const auto dimensions = params[2].split('x');
    if (dimensions.size() != 2)
        return false;

    bool success;
    target->setX(params[0].toDouble(&success));
    if (!success)
        return false;

    target->setY(params[1].toDouble(&success));
    if (!success)
        return false;

    target->setWidth(dimensions[0].toDouble(&success));
    if (!success)
        return false;

    target->setHeight(dimensions[1].toDouble(&success));
    return success;
}

static const auto kArray = QStringLiteral("array");
static const auto kBoolean = QStringLiteral("boolean");
static const auto kNumber = QStringLiteral("number");
static const auto kObject = QStringLiteral("object");
static const auto kString = QStringLiteral("string");
static const auto kNull = QStringLiteral("null");

bool serialize(const QJsonValue::Type& value, QString* target)
{
    switch (value)
    {
        case QJsonValue::Array:
            *target = kArray;
            return true;
        case QJsonValue::Bool:
            *target = kBoolean;
            return true;
        case QJsonValue::Double:
            *target = kNumber;
            return true;
        case QJsonValue::Object:
            *target = kObject;
            return true;
        case QJsonValue::String:
            *target = kString;
            return true;
        case QJsonValue::Null:
            *target = kNull;
            return true;
        case QJsonValue::Undefined:
            return false;
    }
    NX_ASSERT(false, "Unknown `QJsonValue::Type`: %1", (int) value);
    return false;
}

bool deserialize(const QString& value, QJsonValue::Type* target)
{
    *target = nx::utils::switch_(value,
        kArray, [] { return QJsonValue::Array; },
        kBoolean, [] { return QJsonValue::Bool; },
        kNumber, [] { return QJsonValue::Double; },
        kObject, [] { return QJsonValue::Object; },
        kString, [] { return QJsonValue::String; },
        kNull, [] { return QJsonValue::Null; },
        nx::utils::default_, [] { return QJsonValue::Undefined; }
    );
    return *target != QJsonValue::Undefined;
}

bool deserialize(const QString& s, nx::utils::Url* url)
{
    *url = nx::utils::Url(s);
    return true;
}

void serialize(const nx::utils::Url& url, QString* target)
{
    *target = url.toString();
}
