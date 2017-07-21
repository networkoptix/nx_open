#include "to_string.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/core/demangle.hpp>

QString toString(char value)
{
    return QChar::fromLatin1(value);
}

QString toString(wchar_t value)
{
    return QChar(value);
}

QString toString(const char* const value)
{
    return QString::fromUtf8(value);
}

QString toString(const wchar_t* const value)
{
    return toString(std::wstring(value));
}

QString toString(const std::string& value)
{
    return QString::fromStdString(value);
}

QString toString(const std::wstring& value)
{
    return QString::fromStdWString(value);
}

QString toString(const QChar& value)
{
    return value;
}

QString toString(const QByteArray& value)
{
    return QString::fromUtf8(value);
}

QString toString(const QUrl& value)
{
    return value.toString(QUrl::RemovePassword);
}

QString toString(const std::chrono::hours& value)
{
    return QString(QLatin1String("%1h")).arg(value.count());
}

QString toString(const std::chrono::minutes& value)
{
    if (value.count() % 60 == 0)
        return toString(std::chrono::duration_cast<std::chrono::hours>(value));

    return QString(QLatin1String("%1m")).arg(value.count());
}

QString toString(const std::chrono::seconds& value)
{
    if (value.count() % 60 == 0)
        return toString(std::chrono::duration_cast<std::chrono::minutes>(value));

    return QString(QLatin1String("%1s")).arg(value.count());
}

QString toString(const std::chrono::milliseconds& value)
{
    if (value.count() % 1000 == 0)
        return toString(std::chrono::duration_cast<std::chrono::seconds>(value));

    return QString(QLatin1String("%1ms")).arg(value.count());
}

QString toString(const std::chrono::microseconds& value)
{
    if (value.count() % 1000 == 0)
        return toString(std::chrono::duration_cast<std::chrono::milliseconds>(value));

    return QString(QLatin1String("%1usec")).arg(value.count());
}

QString toString(const std::type_info& value)
{
    return demangleTypeName(value.name());
}

QString demangleTypeName(const char* type)
{
    auto typeName = boost::core::demangle(type);

    #if defined(_MSC_VER)
        static const std::vector<std::string> kTypePrefixes = {"struct ", "class "};
        for (const auto& preffix: kTypePrefixes)
        {
            if (boost::starts_with(typeName, preffix))
                typeName = typeName.substr(preffix.size());
        }
    #endif

    return QString::fromStdString(typeName);
}

QString pointerTypeName(const void* /*value*/)
{
    return QLatin1String("void");
}
