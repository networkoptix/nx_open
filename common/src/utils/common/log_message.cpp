/**********************************************************
* Oct 5, 2015
* akolesnikov
***********************************************************/

#include "log_message.h"

#include <utils/common/uuid.h>


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
    return m_str.arg(a, fieldWidth, fillChar);
}

QnLogMessage QnLogMessage::arg(QChar a, int fieldWidth, QChar fillChar) const
{
    return m_str.arg(a, fieldWidth, fillChar);
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

QnLogMessage::operator QString() const
{
    return m_str;
}
