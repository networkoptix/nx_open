/**********************************************************
* Oct 5, 2015
* akolesnikov
***********************************************************/

#ifndef NX_LOG_MESSAGE_H
#define NX_LOG_MESSAGE_H

#include <chrono>
#include <memory>
#include <string>
#include <type_traits>

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QStringList>


class QnUuid;

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

//!Adds some useful overloads to \a QString
class NX_UTILS_API QnLogMessage
{
public:
    QnLogMessage();
    QnLogMessage(const char* str);
    QnLogMessage(const QString& str);
    QnLogMessage(const QByteArray& ba);

    QnLogMessage arg(const QString & a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const;
    QnLogMessage arg(char a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const;
    QnLogMessage arg(const char* a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const;
    QnLogMessage arg(QChar a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const;
    QnLogMessage arg(double a, int fieldWidth = 0, char format = 'g', int precision = -1, QChar fillChar = QLatin1Char(' ')) const;
    QnLogMessage arg(const QByteArray& a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const;
    QnLogMessage arg(const std::string& a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const;
    QnLogMessage arg(const QnUuid& a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const;
    QnLogMessage arg(const void* a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const;
    QnLogMessage arg(const std::chrono::milliseconds a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const;
    QnLogMessage arg(const std::chrono::seconds a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const;
    QnLogMessage arg(const std::chrono::microseconds a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const;

    template<typename T>
    QnLogMessage arg(
        const std::unique_ptr<T>& a,
        int fieldWidth = 0,
        QChar fillChar = QLatin1Char(' '))
    {
        return arg(a.get(), fieldWidth, fillChar);
    }

    template<typename T>
    QnLogMessage arg(
        const std::shared_ptr<T>& a,
        int fieldWidth = 0,
        QChar fillChar = QLatin1Char(' '))
    {
        return arg(a.get(), fieldWidth, fillChar);
    }

    //!Prints integer value
    template<typename T>
    QnLogMessage arg(
        const T& a,
        int fieldWidth = 0,
        int base = 10,
        QChar fillChar = QLatin1Char(' '),
        typename std::enable_if<std::is_integral<T>::value>::type* = 0) const
    {
        return m_str.arg(a, fieldWidth, base, fillChar);
    }


    //!Prints float value
    template<typename T>
    QnLogMessage arg(
        const T& a,
        int fieldWidth = 0,
        char format = 'g',
        int precision = -1,
        QChar fillChar = QLatin1Char(' '),
        typename std::enable_if<std::is_floating_point<T>::value>::type* = 0) const
    {
        return m_str.arg(a, fieldWidth, format, precision, fillChar);
    }

    template<typename T>
    QnLogMessage container(const T& a)
    {
        return arg(
            containerString(
                a,
                QLatin1String(", "),
                QLatin1String(""),
                QLatin1String("") ));
    }

    operator QString() const; 

    std::string toStdString() const { return m_str.toStdString(); }

private:
    QString m_str;
};

//!short name for \a QnLogMessage
typedef QnLogMessage lm;

#endif  //NX_LOG_MESSAGE_H
