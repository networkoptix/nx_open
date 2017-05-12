#include "log_message.h"

namespace nx {
namespace utils {
namespace log {

const QChar Message::kSpace = QLatin1Char(' ');

Message::Message(const QString& text):
    m_str(text)
{
}

Message::Message(const char* text):
    m_str(QString::fromLatin1(text))
{
}

Message::Message(const QByteArray& text):
    m_str(QString::fromUtf8(text))
{
}

Message	Message::arg(int value, int width, int base, const QChar& fill) const
{
    return m_str.arg(value, width, base, fill);
}

Message	Message::arg(uint value, int width, int base, const QChar& fill) const
{
    return m_str.arg(value, width, base, fill);
}

Message	Message::arg(long value, int width, int base, const QChar& fill) const
{
    return m_str.arg(value, width, base, fill);
}

Message	Message::arg(ulong value, int width, int base, const QChar& fill) const
{
    return m_str.arg(value, width, base, fill);
}

Message	Message::arg(qlonglong value, int width, int base, const QChar& fill) const
{
    return m_str.arg(value, width, base, fill);
}

Message	Message::arg(qulonglong value, int width, int base, const QChar& fill) const
{
    return m_str.arg(value, width, base, fill);
}

Message	Message::arg(short value, int width, int base, const QChar& fill) const
{
    return m_str.arg(value, width, base, fill);
}

Message	Message::arg(ushort value, int width, int base, const QChar& fill) const
{
    return m_str.arg(value, width, base, fill);
}

Message	Message::arg(double value, int width, char format, int precision, const QChar& fill) const
{
    return m_str.arg(value, width, format, precision, fill);
}

} // namespace log
} // namespace utils
} // namespace nx
