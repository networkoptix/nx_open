// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <string_view>

#include <QtCore/QByteArray>
#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <nx/utils/buffer.h>

class QnJsonContext;

namespace nx {

/**
 * Single-byte character string to be used across VMS code.
 * Introduces implicit conversions to/from std::string/std::string_view and QString
 * since VMS uses both single-byte and multi-byte strings and nx_network uses std::string.
 * It is recommended to assume that this string always contains UTF-8 text.
 * Can be used for all non-translatable strings.
 */
class NX_VMS_COMMON_API String: public QByteArray
{
    using base_type = QByteArray;

public:
    using base_type::base_type;

    String(const String&) = default;
    String(String&&) = default;

    String& operator=(const String&) = default;
    String& operator=(String&&) = default;

    String(const std::string&);
    String(const std::string_view&);
    String(const QString&);
    String(const char*);

    String(const QByteArray&);
    String(QByteArray&&);

    String(const nx::Buffer&);
    String(nx::Buffer&&);

    String& operator=(const std::string&);
    String& operator=(const std::string_view&);
    String& operator=(const QString&);
    String& operator=(const char*);

    String& operator=(const QByteArray&);
    String& operator=(QByteArray&&);

    String& operator=(const nx::Buffer&);
    String& operator=(nx::Buffer&&);

    operator std::string() const;
    operator std::string_view() const;

    String& operator+=(const QString& s)
    {
        base_type::append(s.toUtf8());
        return *this;
    }

    // NOTE: Deleted to make nx::String assignment to std::string() unambigious.
    operator const char*() const = delete;

    bool empty() const { return isEmpty(); };

    using base_type::append;

    nx::String& append(const std::string_view& str);
    nx::String& append(const nx::Buffer& buf);

    static nx::String number(std::size_t n, int base = 10)
    {
        return base_type::number((qulonglong) n, base);
    }

    template<
        typename Number,
        typename = std::enable_if_t<std::is_integral_v<Number>>
    >
    static nx::String number(Number n, int base = 10)
    {
        return nx::String(base_type::number(n, base));
    }

    template<
        typename Number,
        typename = std::enable_if_t<std::is_floating_point_v<Number>>
    >
    static nx::String number(Number n, char f = 'g', int prec = 6)
    {
        return nx::String(base_type::number(n, f, prec));
    }
};

inline std::ostream& operator<<(std::ostream& os, const String& str)
{
    return os << std::string_view(str.data(), str.size());
}

inline nx::String operator+(const nx::String& left, const nx::String& right)
{
    nx::String result = left;
    result += right;
    return result;
}

inline nx::String operator+(const char* left, const nx::String& right)
{
    nx::String result = left;
    result += right;
    return result;
}

inline nx::String operator+(const nx::String& left, const char* right)
{
    nx::String result = left;
    result += right;
    return result;
}

//-------------------------------------------------------------------------------------------------
// Fusion serialization helpers.

NX_VMS_COMMON_API void serialize(QnJsonContext* ctx, const nx::String& value, QJsonValue* target);
NX_VMS_COMMON_API bool deserialize(QnJsonContext* ctx, const QJsonValue& value, nx::String* target);

} // namespace nx
