// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <bitset>
#include <chrono>
#include <memory>
#include <optional>
#include <type_traits>

#include <QtCore/QSharedPointer>
#include <QtCore/QString>
#include <QtCore/QtDebug>

#include <nx/reflect/enum_string_conversion.h>

//-------------------------------------------------------------------------------------------------
// Forward declarations.

namespace boost {

template<typename T>
class optional;

} // namespace boost

class QUrl;
class QByteArray;
class QJsonValue;
class QJsonArray;
class QJsonObject;

//-------------------------------------------------------------------------------------------------
// Pointers.

namespace nx {

template<typename Iterator>
QString containerString(
    Iterator begin,
    Iterator end,
    const QString& delimiter = ", ",
    const QString& prefix = "{ ",
    const QString& suffix = " }",
    const QString& empty = "none");

NX_UTILS_API QString pointerTypeName(const void* value);

NX_UTILS_API QString demangleTypeName(const char* type);

NX_UTILS_API std::string scopeOfFunction(
    const std::type_info& scopeTagTypeInfo, const char* functionMacro);

template<typename T>
QString pointerTypeName(const T* value);

template<typename T>
QString idForToStringFromPtrSfinae(const T* value, decltype(&T::idForToStringFromPtr));

template<typename T>
QString idForToStringFromPtrSfinae(const T* /*value*/, ...);

//-------------------------------------------------------------------------------------------------
// General.

template<typename T>
QString toString(T&& value);

/**
 * Unwraps std::nested_exceptions and creates the string: "ex1.what(): ex2.what(): ..."
 */
NX_UTILS_API std::string unwrapNestedErrors(const std::exception& e, std::string whats = {});

namespace detail {

template<typename, typename = std::void_t<>, typename = std::void_t<>>
struct IsIteratable: std::false_type {};

template<typename T>
struct IsIteratable<T,
    std::void_t<decltype(std::declval<T>().begin())>,
    std::void_t<decltype(std::declval<T>().end())>
>: std::true_type {};

template<typename... Args>
inline constexpr bool IsIteratableV = IsIteratable<Args...>::value;

//-------------------------------------------------------------------------------------------------

template<typename, typename = std::void_t<>>
struct HasToString: std::false_type {};

template<typename T>
struct HasToString<T,
    std::void_t<decltype(std::declval<T>().toString())>
>: std::true_type {};

template<typename... Args>
inline constexpr bool HasToStringV = HasToString<Args...>::value;

//-------------------------------------------------------------------------------------------------

template<typename T>
QString toStringSfinae(
    const T& value,
    std::enable_if_t<HasToStringV<T>>* = nullptr)
{
    if constexpr (std::is_same<decltype(value.toString()), std::string>::value)
        return QString::fromStdString(value.toString());
    else
        return toString(value.toString());
}

template<typename T>
QString toStringSfinae(
    const T& value,
    std::enable_if_t<std::is_base_of_v<std::exception, T>>* = nullptr)
{
    return QString::fromStdString(unwrapNestedErrors(value));
}

template<typename T>
QString toStringSfinae(
    const T& value,
    std::enable_if_t<std::is_convertible_v<T, QString>>* = nullptr)
{
    return value;
}

template<typename T>
QString toStringSfinae(
    const T& value,
    std::enable_if_t<nx::reflect::IsInstrumentedEnumV<T>>* = nullptr)
{
    return QString::fromStdString(nx::reflect::enumeration::toString(value));
}

// Overload for containers.
template<typename T>
QString toStringSfinae(
    const T& value,
    std::enable_if_t<IsIteratableV<T> && !HasToStringV<T> && !std::is_convertible_v<T, QString>>* = nullptr)
{
    return containerString(value.begin(), value.end());
}

template<typename T>
QString toStringSfinae(const T& value, ...)
{
    QString result;
    QDebug d(&result);
    d.noquote().nospace() << value;
    return result;
}

NX_UTILS_API QString toString(const std::type_info& value);

NX_UTILS_API QString toString(char value);
NX_UTILS_API QString toString(wchar_t value);
NX_UTILS_API QString toString(const char* value);
NX_UTILS_API QString toString(const wchar_t* value);
NX_UTILS_API QString toString(const std::string& value);
NX_UTILS_API QString toString(const std::string_view& value);
NX_UTILS_API QString toString(const std::wstring& value);
NX_UTILS_API QString toString(const std::string_view& value);

NX_UTILS_API QString toString(const QChar& value);
NX_UTILS_API QString toString(const QByteArray& value);
NX_UTILS_API QString toString(const QUrl& value);
NX_UTILS_API QString toString(const QJsonValue& value);
NX_UTILS_API QString toString(const QJsonArray& value);
NX_UTILS_API QString toString(const QJsonObject& value);

NX_UTILS_API QString toString(const std::chrono::hours& value);
NX_UTILS_API QString toString(const std::chrono::minutes& value);
NX_UTILS_API QString toString(const std::chrono::seconds& value);
NX_UTILS_API QString toString(const std::chrono::milliseconds& value);
NX_UTILS_API QString toString(const std::chrono::microseconds& value);
NX_UTILS_API QString toString(const std::chrono::nanoseconds& value);

NX_UTILS_API QString toString(const std::chrono::system_clock::time_point& value);
NX_UTILS_API QString toString(const std::chrono::steady_clock::time_point& value);

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

template<typename T>
QString toString(const QSharedPointer<T>& value)
{
    return toString(value.get());
}

template<typename T>
QString toString(std::reference_wrapper<T> value)
{
    return toString(value.get());
}

//-------------------------------------------------------------------------------------------------
// Templates.

template<typename T>
QString toString(const std::optional<T>& value)
{
    if (value)
        return nx::toString(*value);
    return "none";
}

template<typename First, typename Second>
QString toString(
    const std::pair<First, Second>& pair,
    const QString& prefix = "( ",
    const QString& suffix = " )",
    const QString& delimiter = ": ")
{
    // QString::operator+= works much faster than template substitution.
    QString result = prefix;
    result += nx::toString(pair.first);
    result += delimiter;
    result += nx::toString(pair.second);
    result += suffix;
    return result;
}

template<std::size_t N>
QString toString(const std::bitset<N>& value)
{
    return QString::fromStdString("0b" + value.to_string());
}

/**
 * This function is triggered if no toString(const T&) overload was found using ADL.
 */
template<typename T>
auto toString(const T& value)
{
    return toStringSfinae(value, nullptr);
}

template<typename T>
auto toStringAdl(const T& t)
{
    return toString(t); // ADL kicks in here.
}

} // namespace detail

/**
 * Converts to string any type that supports such conversion.
 * Supported conversions:
 * - String toString(const T&) located in the same namespace as T
 * - String T::toString() const
 * - Overloads for some types (QByteArray, QUrl, std::chrono::*) are provided here.
 * String is something that can be converted to QString using same nx::toString(const String&) call.
 */
template<typename T>
QString toString(T&& value)
{
    // Invoking nx::toString in a recursive manner until we get QString.
    // This is a compile-time recursion, so infinite recursion will be detected at compile time.
    // E.g., if SomeType toString(const SomeType&) is found somewhere.

    if constexpr (std::is_same_v<std::decay_t<T>, QString>)
        return std::forward<T>(value);
    else if constexpr (std::is_same_v<std::decay_t<T>, QLatin1String>)
        return std::forward<T>(value);
    else
        return nx::toString(detail::toStringAdl(value));
}

template<typename Iterator>
QString containerString(
    Iterator begin,
    Iterator end,
    const QString& delimiter,
    const QString& prefix,
    const QString& suffix,
    const QString& empty)
{
    if (begin == end)
        return empty;

    QStringList strings;
    for (auto it = begin; it != end; ++it)
        strings << nx::toString(*it);

    // QString::operator+= works much faster than template subsitution.
    QString result = prefix;
    result += strings.join(delimiter);
    result += suffix;
    return result;
}

template<typename Container>
QString containerString(
    const Container& container,
    const QString& delimiter = ", ",
    const QString& prefix = "{ ",
    const QString& suffix = " }",
    const QString& empty = "none")
{
    return containerString(container.begin(), container.end(), delimiter, prefix, suffix, empty);
}

template<typename T>
QString pointerTypeName(const T* value)
{
    return value ? nx::toString(typeid(*value)) : nx::toString(typeid(T));
}

template<typename T>
QString idForToStringFromPtrSfinae(const T* value, decltype(&T::idForToStringFromPtr))
{
    return value ? nx::toString(value->idForToStringFromPtr()) : QString();
}

template<typename T>
QString idForToStringFromPtrSfinae(const T* /*value*/, ...)
{
    return QString();
}

//-------------------------------------------------------------------------------------------------

inline QString toQString(const std::string& str)
{
    return QString::fromUtf8(str.data(), (int) str.size());
}

inline QString toQString(const std::string_view& str)
{
    return QString::fromUtf8(str.data(), (int) str.size());
}

inline QString toQString(const char* str)
{
    return QString::fromUtf8(str);
}

} // namespace nx
