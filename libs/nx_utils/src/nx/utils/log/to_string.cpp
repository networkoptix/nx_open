#include "to_string.h"

#include <nx/utils/unused.h>
#include <nx/utils/log/log.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/core/demangle.hpp>

// Uncomment to try cache optimization.
// #define NX_UTILS_TO_STRING_CACHE

#if defined(NX_UTILS_TO_STRING_CACHE)
    #include <QReadWriteLock>
    #include <typeindex>
    #include <unordered_map>
#endif

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
    if (nx::utils::log::showPasswords())
        return value.toString();
    else
        return value.toDisplayString();
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

QString demangleTypeName(const char* type)
{
    auto typeName = boost::core::demangle(type);

    #if defined(_MSC_VER)
        static const std::vector<std::string> kTypePrefixes = {"struct ", "class "};
        for (const auto& prefix: kTypePrefixes)
        {
            if (boost::starts_with(typeName, prefix))
                typeName = typeName.substr(prefix.size());
        }
    #endif

    return QString::fromStdString(typeName);
}

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
    const std::type_info& scopeTagTypeInfo, const char* functionMacro)
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
    #else
        nx::utils::unused(functionMacro);
        demangledType = boost::core::demangle(scopeTagTypeInfo.name());
    #endif

    int scopeAfterEndPos = -1; //< Used to trim the function name, keeping the scope.
    int scopeStartPos = 0;
    findFunctionScope(demangledType, &scopeStartPos, &scopeAfterEndPos);

    if (scopeAfterEndPos < 0) //< There is no enclosing scope.
        return "";

    return demangledType.substr(scopeStartPos, scopeAfterEndPos - scopeStartPos);
}
