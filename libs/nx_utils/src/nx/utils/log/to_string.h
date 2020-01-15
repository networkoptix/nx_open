#pragma once

#include <sstream>
#include <bitset>
#include <memory>
#include <chrono>
#include <optional>
#include <type_traits>

#include <boost/optional.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QtDebug>
#include <QtCore/QUrl>

#include <nx/utils/std/optional.h>
#include <nx/utils/exceptions.h>

//-------------------------------------------------------------------------------------------------
// General.

template<typename T>
QString toStringSfinae(const T& value, decltype(&T::toString))
{
    if constexpr (std::is_same<decltype(value.toString()), std::string>::value)
    {
        return value.toString().c_str();
    }
    else
    {
        return value.toString();
    }
}

template<typename T>
QString toStringSfinae(
    const T& value,
    std::enable_if_t<std::is_base_of<std::exception, T>::value>*)
{
    return QString::fromStdString(nx::utils::unwrapNestedErrors(value));
}

template<typename T>
QString toStringSfinae(const T& value, ...)
{
    QString result;
    QDebug d(&result);
    d.noquote().nospace() << value;
    return result;
}

template<typename T>
QString toString(const T& value)
{
    return toStringSfinae(value, nullptr);
}

NX_UTILS_API QString toString(const std::type_info& value);
NX_UTILS_API QString demangleTypeName(const char* type);

NX_UTILS_API QString toString(char value);
NX_UTILS_API QString toString(wchar_t value);
NX_UTILS_API QString toString(const char* value);
NX_UTILS_API QString toString(const wchar_t* value);
NX_UTILS_API QString toString(const std::string& value);
NX_UTILS_API QString toString(const std::wstring& value);

NX_UTILS_API QString toString(const QChar& value);
NX_UTILS_API QString toString(const QByteArray& value);
NX_UTILS_API QString toString(const QUrl& value);

NX_UTILS_API QString toString(const std::chrono::hours& value);
NX_UTILS_API QString toString(const std::chrono::minutes& value);
NX_UTILS_API QString toString(const std::chrono::seconds& value);
NX_UTILS_API QString toString(const std::chrono::milliseconds& value);
NX_UTILS_API QString toString(const std::chrono::microseconds& value);
NX_UTILS_API QString toString(const std::chrono::nanoseconds& value);

template<typename Clock, typename Duration>
QString toString(const std::chrono::time_point<Clock, Duration>& value)
{
    return toString(value.time_since_epoch());
}

//-------------------------------------------------------------------------------------------------
// Pointers.
NX_UTILS_API QString pointerTypeName(const void* value);

NX_UTILS_API std::string scopeOfFunction(
    const std::type_info& scopeTagTypeInfo, const char* functionMacro);

template<typename T>
QString pointerTypeName(const T* value)
{
    return value ? toString(typeid(*value)) : toString(typeid(T));
}

template<typename T>
QString idForToStringFromPtrSfinae(const T* value, decltype(&T::idForToStringFromPtr))
{
    return value ? value->idForToStringFromPtr() : QString();
}

template<typename T>
QString idForToStringFromPtrSfinae(const T* /*value*/, ...)
{
    return QString();
}

template<typename T>
QString toString(const T* value)
{
    const auto toString = idForToStringFromPtrSfinae(value, 0);
    return QStringLiteral("%1(0x%2%3)")
        .arg(pointerTypeName(value)).arg(reinterpret_cast<qulonglong>(value), 0, 16)
        .arg(toString.isEmpty() ? QString() : (QStringLiteral(", ") + toString));
}

template<typename T>
QString toString(T* value)
{
    return toString(const_cast<const T*>(value));
}

template<typename T>
QString toString(const std::unique_ptr<T>& value)
{
    return toString(value.get());
}

template<typename T>
QString toString(const std::shared_ptr<T>& value)
{
    return toString(value.get());
}

//-------------------------------------------------------------------------------------------------
// Templates.

template<typename T>
QString toString(const boost::optional<T>& value)
{
    if (value)
        return toString(value.get());
    return "none";
}

template<typename T>
QString toString(const std::optional<T>& value)
{
    if (value)
        return toString(*value);
    return "none";
}

template<typename First, typename Second>
QString toString(
    const std::pair<First, Second>& pair,
    const QString& prefix = "( ",
    const QString& suffix = " )",
    const QString& delimiter = ": ")
{
    // QString::operator+= works much faster than template subsitution.
    QString result = prefix;
    result += toString(pair.first);
    result += delimiter;
    result += toString(pair.second);
    result += suffix;
    return result;
}

template<std::size_t N>
QString toString(const std::bitset<N>& value)
{
    std::ostringstream result;
    result << "0b" << value;
    return QString::fromStdString(result.str());
}

template<typename Iterator>
QString containerString(
    Iterator begin,
    Iterator end,
    const QString& delimiter = ", ",
    const QString& prefix = "{ ",
    const QString& suffix = " }",
    const QString& empty = "none")
{
    if (begin == end)
        return empty;

    QStringList strings;
    for (auto it = begin; it != end; ++it)
        strings << toString(*it);

    // QString::operator+= works much faster than template subsitution.
    QString result = prefix;
    result += strings.join(delimiter);
    result += suffix;
    return result;
}

template<typename Container>
QString containerString(const Container& container,
    const QString& delimiter = ", ",
    const QString& prefix = "{ ",
    const QString& suffix = " }",
    const QString& empty = "none")
{
    return containerString(container.begin(), container.end(), delimiter, prefix, suffix, empty);
}
