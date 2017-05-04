#pragma once

#include <memory>
#include <chrono>

#include <boost/optional.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtNetwork/QAbstractSocket>

template<typename T>
QString toStringSfinae(const T& t, decltype(&T::toString))
{
    return t.toString();
}

template<typename T>
QString toStringSfinae(const T& t, ...)
{
    return QString(QLatin1String("%1")).arg(t);
}

template<typename T>
QString toString(const T& t)
{
    return toStringSfinae(t, 0);
}

NX_UTILS_API QString toString(const char* s);
NX_UTILS_API QString toString(void* p);
NX_UTILS_API QString toString(const QByteArray& t);
NX_UTILS_API QString toString(const QUrl& url);
NX_UTILS_API QString toString(const std::string& t);

NX_UTILS_API QString toString(const std::chrono::hours& t);
NX_UTILS_API QString toString(const std::chrono::minutes& t);
NX_UTILS_API QString toString(const std::chrono::seconds& t);
NX_UTILS_API QString toString(const std::chrono::milliseconds& t);
NX_UTILS_API QString toString(QAbstractSocket::SocketError error);
 
template<typename T>
QString toString(const std::unique_ptr<T>& p)
{
    return toString((void*) p.get());
}

template<typename T>
QString toString(const std::shared_ptr<T>& p)
{
    return toString((void*) p.get());
}

template<typename T>
QString toString(const boost::optional<T>& t)
{
    if (t)
        return toString(t.get());
    else
        return QLatin1String("boost::none");
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
