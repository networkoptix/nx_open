#include "log_message.h"

constexpr wchar_t QnLogMessage::kSpace;

QnLogMessage::QnLogMessage()
{
}

QnLogMessage::QnLogMessage(const char* text)
:
    m_str(QString::fromLatin1(text))
{
}

QnLogMessage::QnLogMessage(const QString& text)
:
    m_str(text)
{
}

QnLogMessage::QnLogMessage(const QByteArray& text)
:
    m_str(QString::fromLatin1(text))
{
}

QnLogMessage QnLogMessage::arg(const QByteArray& a, int fieldWidth, wchar_t fillChar) const
{
    return m_str.arg(QString::fromLatin1(a), fieldWidth, fillChar);
}

QnLogMessage QnLogMessage::arg(const std::string& a, int fieldWidth, wchar_t fillChar) const
{
    return m_str.arg(QString::fromStdString(a), fieldWidth, fillChar);
}

QnLogMessage QnLogMessage::arg(const QnUuid& a, int fieldWidth, wchar_t fillChar) const
{
    return m_str.arg(a.toString(), fieldWidth, fillChar);
}

QnLogMessage QnLogMessage::arg(std::chrono::seconds a, int fieldWidth, wchar_t fillChar) const
{
    return m_str.arg(QString::fromLatin1("%1s").arg(a.count()), fieldWidth, fillChar);
}

QnLogMessage QnLogMessage::arg(std::chrono::milliseconds a, int fieldWidth, wchar_t fillChar) const
{
    return m_str.arg(QString::fromLatin1("%1ms").arg(a.count()), fieldWidth, fillChar);
}

QnLogMessage QnLogMessage::arg(std::chrono::microseconds a, int fieldWidth, wchar_t fillChar) const
{
    return m_str.arg(QString::fromLatin1("%1usec").arg(a.count()), fieldWidth, fillChar);
}

QnLogMessage::operator QString() const
{
    return m_str;
}
