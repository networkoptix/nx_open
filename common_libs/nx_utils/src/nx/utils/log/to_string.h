#pragma once

#include <memory>
#include <chrono>

#include <boost/optional.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QtDebug>
#include <QtCore/QUrl>

// ------------------------------------------------------------------------------------------------
// General.

template<typename T>
QString toStringSfinae(const T& value, decltype(&T::toString))
{
    return value.toString();
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
    return toStringSfinae(value, 0);
}

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

// ------------------------------------------------------------------------------------------------
// Pointers.

NX_UTILS_API QString toString(const std::type_info& value);
NX_UTILS_API QString demangleTypeName(const char* type);
NX_UTILS_API QString pointerTypeName(const void* value);

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
    return toString((const T*) value);
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

// ------------------------------------------------------------------------------------------------
// Templates.

template<typename T>
QString toString(const boost::optional<T>& value)
{
    if (value)
        return toString(value.get());
    else
        return QLatin1String("none");
}

template<typename First, typename Second>
QString toString(
    const std::pair<First, Second>& pair,
    const QString& prefix = QString::fromLatin1("( "),
    const QString& suffix = QString::fromLatin1(" )"),
    const QString& delimiter = QString::fromLatin1(": "))
{
    return QString::fromLatin1("%1%2%3%4%5").arg(prefix)
        .arg(toString(pair.first)).arg(delimiter)
        .arg(toString(pair.second)).arg(suffix);
}

template<typename Container>
QString containerString(const Container& container,
    const QString& delimiter = QString::fromLatin1(", "),
    const QString& prefix = QString::fromLatin1("{ "),
    const QString& suffix = QString::fromLatin1(" }"),
    const QString& empty = QString::fromLatin1("none"))
{
    if (container.begin() == container.end())
        return empty;

    QStringList strings;
    for (const auto& item : container)
        strings << toString(item);

    return QString::fromLatin1("%1%2%3").arg(prefix).arg(strings.join(delimiter)).arg(suffix);
}
