// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "string.h"

#include <nx/fusion/serialization/json_functions.h>

namespace nx {

String::String(const std::string& str):
    QByteArray(QByteArray::fromStdString(str))
{
}

String::String(const std::string_view& str):
    QByteArray(str.data(), str.size())
{
}

String::String(const QString& str):
    QByteArray(str.toUtf8())
{
}

String::String(const char* str):
    QByteArray(str)
{
}

String::String(const QByteArray& str):
    QByteArray(str)
{
}

String::String(QByteArray&& str) :
    QByteArray(std::move(str))
{
}

String::String(const nx::Buffer& buf):
    QByteArray(buf.toByteArray())
{
}

String::String(nx::Buffer&& buf):
    QByteArray(buf.takeByteArray())
{
}

String& String::operator=(const std::string& str)
{
    *this = QByteArray(str.data(), str.size());
    return *this;
}

String& String::operator=(const std::string_view& str)
{
    *this = QByteArray(str.data(), str.size());
    return *this;
}

String& String::operator=(const QString& str)
{
    *this = str.toUtf8();
    return *this;
}

String& String::operator=(const char* str)
{
    static_cast<QByteArray&>(*this) = str;
    return *this;
}

String& String::operator=(const QByteArray& str)
{
    static_cast<QByteArray&>(*this) = str;
    return *this;
}

String& String::operator=(QByteArray&& str)
{
    static_cast<QByteArray&>(*this) = std::move(str);
    return *this;
}

String& String::operator=(const nx::Buffer& str)
{
    return *this = str.toByteArray();
}

String& String::operator=(nx::Buffer&& str)
{
    return *this = str.takeByteArray();
}

String::operator std::string() const
{
    return std::string(constData(), size());
}

String::operator std::string_view() const
{
    return std::string_view(constData(), size());
}

nx::String& String::append(const std::string_view& str)
{
    base_type::append(str.data(), (int) str.size());
    return *this;
}

nx::String& String::append(const nx::Buffer& buf)
{
    base_type::append(buf.data(), (int) buf.size());
    return *this;
}

//-------------------------------------------------------------------------------------------------
// Fusion serialization helpers.

void serialize(QnJsonContext* ctx, const nx::String& value, QJsonValue* target)
{
    ::serialize(ctx, QString::fromUtf8(value), target);
}

bool deserialize(QnJsonContext* ctx, const QJsonValue& value, nx::String* target)
{
    QString str;
    if (!::deserialize(ctx, value, &str))
        return false;

    *target = str.toUtf8();
    return true;
}

} // namespace nx
