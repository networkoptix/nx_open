// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "to_string.h"

#include <time.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/core/demangle.hpp>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonValue>
#include <QtCore/QUrl>

#include <nx/utils/exception.h>

// Uncomment to try cache optimization.
// #define NX_UTILS_TO_STRING_CACHE

#if defined(NX_UTILS_TO_STRING_CACHE)
    #include <QReadWriteLock>
    #include <typeindex>
    #include <unordered_map>
#endif

#include <nx/utils/log/log.h>

namespace nx {

QString demangleTypeName(const char* type)
{
    #if defined(_MSC_VER)
        static constexpr const char* kTypePrefixes[] = {"struct ", "class "};
        static const size_t kTypePrefixesSize[] = {
            strlen(kTypePrefixes[0]),
            strlen(kTypePrefixes[1]),
        };

        QString result;
        result.reserve(strlen(type)); //< Result is not shorter than the input.

        const char* p = type;
        for (int i = 0; i < (int) (sizeof(kTypePrefixes) / sizeof(kTypePrefixes[0])); ++i)
        {
            if (strncmp(type, kTypePrefixes[i], kTypePrefixesSize[i]) == 0)
            {
                p += kTypePrefixesSize[i];
                break;
            }
        }

        while (*p != '\0')
        {
            const char c = *p;
            result += c;
            if (c == '<' || c == ' ' || c == ',')
            {
                for (int i = 0; i < (int) (sizeof(kTypePrefixes) / sizeof(kTypePrefixes[0])); ++i)
                {
                     if (strncmp(p + 1, kTypePrefixes[i], kTypePrefixesSize[i]) == 0)
                     {
                         p += kTypePrefixesSize[i];
                         break;
                     }
                }
            }
            ++p;
        }

        return result;
    #else
        return QString::fromStdString(boost::core::demangle(type));
    #endif
}

std::string unwrapNestedErrors(const std::exception& e, std::string whats)
{
    if (whats.size() > 0)
        whats += ": ";
    whats += e.what();

    try
    {
        std::rethrow_if_nested(e);
    }
    catch (const std::exception& e)
    {
       return unwrapNestedErrors(e, std::move(whats));
    }
    return whats;
}

namespace detail {

QString toString(const std::type_info& value)
{
    #if defined NX_UTILS_TO_STRING_CACHE
        static QReadWriteLock mutex;
        static std::unordered_map<std::type_index, QString> cache;

        {
            QReadLocker lock(&mutex);
            const auto it = cache.find(value);
            if (it != cache.end())
                return it->second;
        }

        QWriteLocker lock(&mutex);
        auto& s = cache[value];
        if (s.isEmpty())
            s = demangleTypeName(value.name());

        return s;
    #else
        return demangleTypeName(value.name());
    #endif
}

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

QString toString(const std::string_view& value)
{
    return QString::fromUtf8(value.data(), (int) value.size());
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
    if (nx::log::showPasswords())
        return value.toString();
    else
        return value.toDisplayString();
}

QString toString(const QJsonValue& value)
{
    switch (value.type())
    {
        case QJsonValue::Undefined:
            break;

        case QJsonValue::Null:
            return "null";

        case QJsonValue::Bool:
        case QJsonValue::Double:
        case QJsonValue::String:
            return value.toVariant().toString();

        case QJsonValue::Array:
        case QJsonValue::Object:
            return QJsonDocument::fromVariant(value.toVariant()).toJson(QJsonDocument::Compact);
    };

    NX_ASSERT(false, "Unsupported type: %1", static_cast<int>(value.type()));
    return NX_FMT("UnsupportedType(%1)", static_cast<int>(value.type()));
}

NX_UTILS_API QString toString(const QJsonArray& value)
{
    return toString(QJsonValue(value));
}

NX_UTILS_API QString toString(const QJsonObject& value)
{
    return toString(QJsonValue(value));
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

    return QString(QLatin1String("%1us")).arg(value.count());
}

QString toString(const std::chrono::nanoseconds &value)
{
    if (value.count() % 1000 == 0)
        return toString(std::chrono::duration_cast<std::chrono::microseconds>(value));

    return QString(QLatin1String("%1ns")).arg(value.count());
}

QString toString(const std::chrono::system_clock::time_point& value)
{
    using namespace std::chrono;

    return QDateTime::fromMSecsSinceEpoch(
        duration_cast<milliseconds>(value.time_since_epoch()).count()).toString(Qt::ISODate);
}

QString toString(const std::chrono::steady_clock::time_point& value)
{
    using namespace std::chrono;

    return "steady: " + toString(duration_cast<milliseconds>(value.time_since_epoch()));
}

} // namespace detail

QString pointerTypeName(const void* /*value*/)
{
    return QLatin1String("void");
}

/**
 * Mark the substring of the function type which defines the function class/namespace scope.
 * @param outScopeAfterEndPos Set to -1 if the scope is global (the scope string is empty).
 */
static void findFunctionScope(
    const std::string& demangledType, int* outScopeStartPos, int* outScopeAfterEndPos)
{
    // The scope ends before the last "::" which appears before the first "(" outside "<...>"
    // (this "(" denotes the start of the function arguments), or, for lambdas on MSVC, before the
    // first "<" following "::" (this "<" starts the construction like "<lambda_...>"). The scope
    // starts after the last space outside "<...>" (this space precedes the function name with its
    // possible scope), or, if there is no such space, at the beginning of the source string.

    int anglesNestingLevel = 0;
    *outScopeAfterEndPos = -1; //< Will be updated as we parse the string.
    *outScopeStartPos = 0; //< Will be updated as we parse the string.
    for (int p = 0; p < (int) demangledType.size(); ++p)
    {
        switch (demangledType.at(p))
        {
            case '(':
                if (anglesNestingLevel == 0)
                    return;
                break;
            case ' ':
                if (anglesNestingLevel == 0
                    // Ignore spaces immediately followed by '(' - found on MSVC in 'operator ()'.
                    && demangledType.substr(p + 1, 1) != "(")
                {
                    *outScopeStartPos = p + 1;
                    // Ignore all "::" which may have appeared before we found the space.
                    *outScopeAfterEndPos = -1;
                }
                break;
            case ':':
                if (anglesNestingLevel == 0 && p > 0 && demangledType.at(p - 1) == ':')
                {
                    if (demangledType.substr(p + 1, 1) == "<") //< MSVC: "::<lambda_...>"
                        return;
                    *outScopeAfterEndPos = p - 1; //< Store the position of "::".
                }
                break;
            case '<':
            {
                ++anglesNestingLevel;
                break;
            }
            case '>':
                --anglesNestingLevel;
                break;
        }
    }
}

/**
 * Constructs the string which specifies the namespace/class scope of a particular function.
 * @param scopeTagTypeInfo Type info for any struct defined in the function.
 * @param functionMacro Value of __FUNCTION__ inside the function.
 * @return Function name with namespaces and outer classes (if any), including template arguments
 *     in case an outer class or the function is a template. Return type and function arguments are
 *     not included.
 */
std::string scopeOfFunction(
    const std::type_info& scopeTagTypeInfo, [[maybe_unused]] const char* functionMacro)
{
    std::string demangledType;
    #if defined(_MSC_VER)
        if (boost::ends_with(functionMacro, "::operator ()")) //< MSVC lambda.
        {
            // On MSVC, type_info::name() for types defined in a lambda does not contain the scope
            // of the lambda, thus, we have to use __FUNCTION__ instead.
            demangledType = functionMacro;
        }
        else
        {
            // Convert the type from the form of type_info::name() to the form of __FUNCTION__.
            demangledType = scopeTagTypeInfo.name();
            boost::replace_all(demangledType, "* __ptr64", "*");
        }

        // Remove unnamed namespaces from the scope.
        boost::replace_all(demangledType, "`anonymous namespace'::", "");
    #else
        demangledType = boost::core::demangle(scopeTagTypeInfo.name());
        // Remove unnamed namespaces from the scope.
        boost::replace_all(demangledType, "(anonymous namespace)::", "");
    #endif

    int scopeAfterEndPos = -1; //< Used to trim the function name, keeping the scope.
    int scopeStartPos = 0;
    findFunctionScope(demangledType, &scopeStartPos, &scopeAfterEndPos);

    if (scopeAfterEndPos < 0) //< There is no enclosing scope.
        return "";

    return demangledType.substr(scopeStartPos, scopeAfterEndPos - scopeStartPos);
}

} // namespace nx
