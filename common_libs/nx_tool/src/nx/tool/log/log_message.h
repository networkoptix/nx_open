/**********************************************************
* Oct 5, 2015
* akolesnikov
***********************************************************/

#ifndef NX_LOG_MESSAGE_H
#define NX_LOG_MESSAGE_H

#include <string>
#include <type_traits>

#include <QtCore/QByteArray>
#include <QtCore/QString>


class QnUuid;

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

    return lit("%1%2%3").arg(prefix).arg(strings.join(delimiter)).arg(suffix);
}

//!Adds some useful overloads to \a QString
class NX_TOOL_API QnLogMessage
{
public:
    QnLogMessage();
    QnLogMessage(const char* str);
    QnLogMessage(const QString& str);
    QnLogMessage(const QByteArray& ba);

    QnLogMessage arg(const QString & a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const;
    QnLogMessage arg(char a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const;
    QnLogMessage arg(QChar a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const;
    QnLogMessage arg(double a, int fieldWidth = 0, char format = 'g', int precision = -1, QChar fillChar = QLatin1Char(' ')) const;
    QnLogMessage arg(const QByteArray& a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const;
    QnLogMessage arg(const std::string& a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const;
    QnLogMessage arg(const QnUuid& a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const;

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
        return arg(containerString(a, lit(", "), lit(""), lit("") ));
    }

    operator QString() const;

private:
    QString m_str;
};

//!short name for \a QnLogMessage
typedef QnLogMessage lm;

#endif  //NX_LOG_MESSAGE_H
