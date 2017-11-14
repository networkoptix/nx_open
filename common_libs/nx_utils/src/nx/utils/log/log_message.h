#pragma once

#include <string>
#include <type_traits>

#include <nx/utils/log/to_string.h>

namespace nx {
namespace utils {
namespace log {

/**
 * Universal message formatter with QString::arg interface.
 *
 * NOTE:: Do not add any function overloads for aditional types support. Rather implement a
 * member function 'QString toString() const' or external function 'QString toString(const T&)'
 * so it will be supported automatically.
 */
class NX_UTILS_API Message
{
public:
    static const QChar kSpace;

    Message(const QString& text = QString());
    Message(const char* text);
    Message(const QByteArray& text);

    operator QString() const { return m_str; }
    QString toQString() const { return m_str; }
    QByteArray toUtf8() const { return m_str.toUtf8(); }
    std::string toStdString() const { return m_str.toStdString(); }

    template<typename Value>
    Message arg(const Value& value, int width = 0, const QChar& fill = kSpace) const
    {
        using ::toString;
        return m_str.arg(toString(value), width, fill);
    }

    template<typename ... Values>
    Message args(const Values& ... values) const
    {
        using ::toString;
        return m_str.arg(toString(values) ...);
    }

    template<typename ... Arguments>
    Message container(const Arguments& ... arguments) const
    {
        return m_str.arg(containerString(arguments ...));
    }

    // QString number format compatibility.
    Message	arg(int value, int width = 0, int base = 10, const QChar& fill = kSpace) const;
    Message	arg(uint value, int width = 0, int base = 10, const QChar& fill = kSpace) const;
    Message	arg(long value, int width = 0, int base = 10, const QChar& fill = kSpace) const;
    Message	arg(ulong value, int width = 0, int base = 10, const QChar& fill = kSpace) const;
    Message	arg(qlonglong value, int width = 0, int base = 10, const QChar& fill = kSpace) const;
    Message	arg(qulonglong value, int width = 0, int base = 10, const QChar& fill = kSpace) const;
    Message	arg(short value, int width = 0, int base = 10, const QChar& fill = kSpace) const;
    Message	arg(ushort value, int width = 0, int base = 10, const QChar& fill = kSpace) const;
    Message	arg(double value, int width = 0, char format = 'g', int precision = -1,
        const QChar& fill = kSpace) const;

private:
    QString m_str;
};

} // namespace log
} // namespace utils
} // namespace nx

// TODO: Move to namespace nx (at least).
typedef nx::utils::log::Message lm;
