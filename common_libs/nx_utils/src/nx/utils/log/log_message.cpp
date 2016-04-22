/**********************************************************
* Oct 5, 2015
* akolesnikov
***********************************************************/

#include "log_message.h"

#include <nx/utils/uuid.h>


QnLogMessage::QnLogMessage()
{
}

QnLogMessage::QnLogMessage(const char* str)
:
    m_str(QString::fromLatin1(str))
{
}

QnLogMessage::QnLogMessage(const QString& str)
:
    m_str(str)
{
}

QnLogMessage::QnLogMessage(const QByteArray& ba)
:
    m_str(QString::fromLatin1(ba))
{
}

QnLogMessage QnLogMessage::arg(const QString& a, int fieldWidth, QChar fillChar) const
{
    return m_str.arg(a, fieldWidth, fillChar);
}

QnLogMessage QnLogMessage::arg(char a, int fieldWidth, QChar fillChar) const
{
    return m_str.arg(QChar::fromLatin1(a), fieldWidth, fillChar);
}

QnLogMessage QnLogMessage::arg(const char* a, int fieldWidth, QChar fillChar) const
{
    return m_str.arg(QString::fromUtf8(a), fieldWidth, fillChar);
}

QnLogMessage QnLogMessage::arg(QChar a, int fieldWidth, QChar fillChar) const
{
    return m_str.arg(a, fieldWidth, fillChar);
}

QnLogMessage QnLogMessage::arg(double a, int fieldWidth, char format, int precision, QChar fillChar) const
{
    return m_str.arg(a, fieldWidth, format, precision, fillChar);
}

QnLogMessage QnLogMessage::arg(const QByteArray& a, int fieldWidth, QChar fillChar) const
{
    return m_str.arg(QString::fromLatin1(a), fieldWidth, fillChar);
}

QnLogMessage QnLogMessage::arg(const std::string& a, int fieldWidth, QChar fillChar) const
{
    return m_str.arg(QString::fromStdString(a), fieldWidth, fillChar);
}

QnLogMessage QnLogMessage::arg(const QnUuid& a, int fieldWidth, QChar fillChar) const
{
    return m_str.arg(a.toString(), fieldWidth, fillChar);
}

QnLogMessage QnLogMessage::arg(const void* a, int fieldWidth, QChar fillChar) const
{
    return m_str.arg(QString::fromLatin1("0x%1").arg(
        reinterpret_cast<qulonglong>(a), fieldWidth - 2, 16, fillChar));
}

QnLogMessage QnLogMessage::arg(const std::chrono::milliseconds a, int fieldWidth, QChar fillChar) const
{
    return m_str.arg(QString::fromLatin1("%1ms").arg(a.count()), fieldWidth, fillChar);
}

QnLogMessage QnLogMessage::arg(const std::chrono::seconds a, int fieldWidth, QChar fillChar) const
{
    return m_str.arg(QString::fromLatin1("%1s").arg(a.count()), fieldWidth, fillChar);
}

QnLogMessage QnLogMessage::arg(const std::chrono::microseconds a, int fieldWidth, QChar fillChar) const
{
    return m_str.arg(QString::fromLatin1("%1usec").arg(a.count()), fieldWidth, fillChar);
}

QnLogMessage::operator QString() const
{
    return m_str;
}
