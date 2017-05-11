#include "log_message.h"

constexpr wchar_t QnLogMessage::kSpace;

namespace {

template <typename T>
QString debug(const T& value)
{
    QString result;
    QDebug d(&result);
    d << value;
    return result;
}

} // namespace

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

QnLogMessage QnLogMessage::arg(const char* str, int fieldWidth, wchar_t fillChar) const
{
    return m_str.arg(QString::fromUtf8(str), fieldWidth, fillChar);
}

QnLogMessage QnLogMessage::arg(const wchar_t* str, int fieldWidth, wchar_t fillChar) const
{
    return m_str.arg(QString::fromWCharArray(str), fieldWidth, fillChar);
}

QnLogMessage QnLogMessage::arg(const QnUuid& a, int fieldWidth, wchar_t fillChar) const
{
    return m_str.arg(a.toString(), fieldWidth, fillChar);
}

QnLogMessage QnLogMessage::arg(const QUrl& a, int fieldWidth, wchar_t fillChar) const
{
    return m_str.arg(toString(a), fieldWidth, fillChar);
}

QnLogMessage QnLogMessage::arg(const QPoint& point) const
{
    return m_str.arg(debug(point));
}

QnLogMessage QnLogMessage::arg(const QPointF& point) const
{
    return m_str.arg(debug(point));
}

QnLogMessage QnLogMessage::arg(const QSize& size) const
{
    return m_str.arg(debug(size));
}

QnLogMessage QnLogMessage::arg(const QSizeF& size) const
{
    return m_str.arg(debug(size));
}

QnLogMessage QnLogMessage::arg(const QRect& rect) const
{
    return m_str.arg(debug(rect));
}

QnLogMessage QnLogMessage::arg(const QRectF& rect) const
{
    return m_str.arg(debug(rect));
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

std::string QnLogMessage::toStdString() const
{
    return m_str.toStdString();
}
