#pragma once

#include <string>
#include <type_traits>

#include <QtCore/QByteArray>
#include <QtCore/QStringList>

#include <nx/utils/log/to_string.h>
#include <nx/utils/uuid.h>

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

    template<typename T>
    QnLogMessage arg(
        const boost::optional<T>& a,
        int fieldWidth = 0,
        QChar fillChar = QLatin1Char(' '))
    {
        return arg(toString(a), fieldWidth, fillChar);
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

    template<typename T, typename ... Args>
    QnLogMessage str(const T& a, Args ... args)
    {
        return arg(toString(a, std::forward(args) ...));
    }

    template<typename T, typename ... Args>
    QnLogMessage container(const T& a, Args ... args)
    {
        return arg(containerString(a, std::forward(args) ...));
    }

    template<typename T, typename ... Args>
    QnLogMessage args(const T& a, Args ... args)
    {
        return arg(a).args(std::forward<Args>(args) ...);
    }

    template<typename T>
    QnLogMessage args(const T& a)
    {
        return arg(a);
    }

    template<typename T, typename ... Args>
    QnLogMessage strs(const T& a, Args ... args)
    {
        return strs(a).strs(std::forward<Args>(args) ...);
    }

    template<typename T>
    QnLogMessage strs(const T& a)
    {
        return str(a);
    }

    operator QString() const;

    std::string toStdString() const { return m_str.toStdString(); }

private:
    QString m_str;
};

//!short name for \a QnLogMessage
typedef QnLogMessage lm;
