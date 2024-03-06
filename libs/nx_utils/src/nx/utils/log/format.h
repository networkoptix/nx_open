// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

// TODO: Move to nx/formatter.h

#include <string>
#include <type_traits>

#include <nx/utils/general_macros.h>
#include <nx/utils/log/to_string.h>

namespace nx {

/**
 * Universal message formatter with QString::arg interface.
 *
 * NOTE: Do not add any function overloads for additional types support. Rather implement a
 * member function 'QString toString() const' or external function 'QString toString(const T&)'
 * so it will be supported automatically.
 */
class NX_UTILS_API Formatter
{
public:
    static const QChar kSpace;

    Formatter(const QString& text = QString());
    Formatter(const char* text);
    Formatter(const QByteArray& text);

    operator QString() const { return m_str; }
    QString toQString() const { return m_str; }
    QByteArray toUtf8() const { return m_str.toUtf8(); }
    std::string toStdString() const { return m_str.toStdString(); }

    template<typename Value>
    Formatter arg(const Value& value, int width = 0, const QChar& fill = kSpace) const
    {
        return m_str.arg(nx::toString(value), width, fill);
    }

    template<typename ... Values>
    Formatter args(const Values& ... values) const
    {
        // Double toString() call is needed because some of toString() implementations (e.g. some
        // SDK toString() that convert their arguments to an std::string) don't return a QString
        // directly but something that can be converted to a QString with the second toString()
        // call.
        return m_str.arg(nx::toString(nx::toString(values)) ...);
    }

    template<typename... Values>
    Formatter args(const std::tuple<Values...>& values) const
    {
        return std::apply(
            [this](auto&&... values) { return args(std::forward<decltype(values)>(values)...); },
            values);
    }

    template<typename ... Arguments>
    Formatter container(const Arguments& ... arguments) const
    {
        return m_str.arg(containerString(arguments ...));
    }

    // QString number format compatibility.
    Formatter arg(const std::string& s) const { return arg(s.c_str()); };
    Formatter arg(int value, int width = 0, int base = 10, const QChar& fill = kSpace) const;
    Formatter arg(uint value, int width = 0, int base = 10, const QChar& fill = kSpace) const;
    Formatter arg(long value, int width = 0, int base = 10, const QChar& fill = kSpace) const;
    Formatter arg(ulong value, int width = 0, int base = 10, const QChar& fill = kSpace) const;
    Formatter arg(qlonglong value, int width = 0, int base = 10, const QChar& fill = kSpace) const;
    Formatter arg(qulonglong value, int width = 0, int base = 10, const QChar& fill = kSpace) const;
    Formatter arg(short value, int width = 0, int base = 10, const QChar& fill = kSpace) const;
    Formatter arg(ushort value, int width = 0, int base = 10, const QChar& fill = kSpace) const;
    Formatter arg(double value, int width = 0, char format = 'g', int precision = -1, const QChar& fill = kSpace) const;

private:
    QString m_str;
};

inline Formatter format() { return Formatter{}; }

template<typename Format>
Formatter format(const Format& format) { return Formatter(nx::toString(format)); }

/** Use when the format string is not a literal: `return format(formatString, name, value);` */
template<typename Format, typename ... Args>
Formatter format(Format format, const Args& ... args)
{
    return Formatter(std::forward<Format>(format)).args(args...);
}

inline
void staticAssertLiteral() {}

template<int N>
void staticAssertLiteral(const char (&)[N]) {}

} // namespace nx

#define NX_DETAIL_LITERAL(STRING) (::nx::staticAssertLiteral(STRING), QStringLiteral(STRING))
#define NX_DETAIL_FORMAT_LITERAL(STRING) ::nx::format(NX_DETAIL_LITERAL(STRING))
#define NX_DETAIL_FORMAT(FORMAT, ...) ::nx::format(NX_DETAIL_LITERAL(FORMAT), __VA_ARGS__)

/**
 * Universal optimized string formatter. To be used when the format is a string literal.
 *
 * Usage:
 * ```
 *     QString name = NX_FMT("propertyName");
 *     QString value = NX_FMT("[%1, %2]", first, second);
 * ```
 */
#define NX_FMT(...) \
    /* Choose one of the two macros depending on the number of args supplied to this macro. */ \
    NX_MSVC_EXPAND(NX_GET_14TH_ARG( \
        __VA_ARGS__, \
        /* Repeat 12 times: allow for up to 11 args which are %N-substituted into the message. */ \
        NX_DETAIL_FORMAT, NX_DETAIL_FORMAT, NX_DETAIL_FORMAT, NX_DETAIL_FORMAT, NX_DETAIL_FORMAT, \
        NX_DETAIL_FORMAT, NX_DETAIL_FORMAT, NX_DETAIL_FORMAT, NX_DETAIL_FORMAT, NX_DETAIL_FORMAT, \
        NX_DETAIL_FORMAT, NX_DETAIL_FORMAT, \
        NX_DETAIL_FORMAT_LITERAL, /*< Chosen when called with message only. */ \
        args_required /*< Chosen when called without arguments; leads to an error. */ \
    )(__VA_ARGS__))
