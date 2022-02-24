// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "format.h"

namespace nx {

const QChar Formatter::kSpace = QLatin1Char(' ');

Formatter::Formatter(const QString& text):
    m_str(text)
{
}

Formatter::Formatter(const char* text):
    m_str(QString::fromLatin1(text))
{
}

Formatter::Formatter(const QByteArray& text):
    m_str(QString::fromUtf8(text))
{
}

Formatter Formatter::arg(int value, int width, int base, const QChar& fill) const
{
    return m_str.arg(value, width, base, fill);
}

Formatter Formatter::arg(uint value, int width, int base, const QChar& fill) const
{
    return m_str.arg(value, width, base, fill);
}

Formatter Formatter::arg(long value, int width, int base, const QChar& fill) const
{
    return m_str.arg(value, width, base, fill);
}

Formatter Formatter::arg(ulong value, int width, int base, const QChar& fill) const
{
    return m_str.arg(value, width, base, fill);
}

Formatter Formatter::arg(qlonglong value, int width, int base, const QChar& fill) const
{
    return m_str.arg(value, width, base, fill);
}

Formatter Formatter::arg(qulonglong value, int width, int base, const QChar& fill) const
{
    return m_str.arg(value, width, base, fill);
}

Formatter Formatter::arg(short value, int width, int base, const QChar& fill) const
{
    return m_str.arg(value, width, base, fill);
}

Formatter Formatter::arg(ushort value, int width, int base, const QChar& fill) const
{
    return m_str.arg(value, width, base, fill);
}

Formatter Formatter::arg(double value, int width, char format, int precision, const QChar& fill) const
{
    return m_str.arg(value, width, format, precision, fill);
}

} // namespace nx
