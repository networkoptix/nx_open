// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <algorithm>
#include <climits>
#include <cerrno>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>
#include <map>

#if defined(_WIN32)
    #define NOMINMAX //< Needed to prevent windows.h define macros min() and max().
    #include <windows.h>
    #include <Windows.h>
    #include <shellapi.h>
#elif defined(__APPLE__)
    #include <nx/kit/apple_utils.h>
#else
    #include <fstream>
#endif

namespace nx {
namespace kit {
namespace utils {

std::string baseName(std::string path)
{
    #if defined(_WIN32)
        const std::string pathWithoutDrive = (path.size() >= 2
             && ((path[0] >= 'a' && path[0] <= 'z') || (path[0] >= 'A' && path[0] <= 'Z'))
             && path[1] == ':')
                 ? path.substr(2)
                 : path;

        const size_t slashPos = pathWithoutDrive.rfind('/');
        const size_t backslashPos = pathWithoutDrive.rfind('\\');

        if (slashPos == std::string::npos && backslashPos == std::string::npos)
            return pathWithoutDrive;

        const size_t lastPathSeparator = (slashPos == std::string::npos)
            ? backslashPos
            : (backslashPos == std::string::npos)
                ? slashPos
                : std::max(slashPos, backslashPos);

        return pathWithoutDrive.substr(lastPathSeparator + 1);
    #else
        const size_t slashPos = path.rfind('/');
        return (slashPos == std::string::npos) ? path : path.substr(slashPos + 1);
    #endif
}

std::string absolutePath(
    const std::string& originDir, const std::string& path)
{
    if (originDir.empty())
        return path;
    if (path.empty())
        return originDir;

    if (path[0] == '/')
        return path;

    #if defined(_WIN32)
        if (path[0] == '\\')
            return path;
        if (path.size() >= 2
            && ((path[0] >= 'a' && path[0] <= 'z') || (path[0] >= 'A' && path[0] <= 'Z'))
            && path[1] == ':')
        {
            return path;
        }
    #endif

    char separator = '/';

    if (originDir[originDir.size() - 1] == '/')
        separator = '\0'; //< The separator is not needed.

    #if defined(_WIN32)
        if (originDir[originDir.size() - 1] == '\\')
            separator = '\0'; //< The separator is not needed.
        if (separator)
        {
            if (path.find('\\') != std::string::npos)
                separator = '\\'; //< Change to backslash if backslashes exist in path.
            else if (path.find('/') == std::string::npos
                && originDir.find('\\') != std::string::npos)
            {
                // Neither slash nor backslash exists in path, but backslash exists in the origin.
                separator = '\\';
            }
        }
    #endif

    if (separator)
        return originDir + separator + path;

    return originDir + path;
}

std::string getProcessName()
{
    std::string processName =
        nx::kit::utils::baseName(nx::kit::utils::getProcessCmdLineArgs()[0]);

    #if defined(_WIN32)
        const std::string exeExt = ".exe";
        if (processName.size() > exeExt.size()
            && processName.substr(processName.size() - exeExt.size()) == exeExt)
        {
            processName = processName.substr(0, processName.size() - exeExt.size());
        }
    #endif

    return processName;
}

const std::vector<std::string>& getProcessCmdLineArgs()
{
    static std::vector<std::string> args;

    if (!args.empty())
        return args;

    #if defined(_WIN32)
        int argc;
        LPWSTR* const argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!argv || argc == 0)
        {
            LocalFree(argv);
            args = std::vector<std::string>{""};
            return args;
        }

        for (int i = 0; i < argc; ++i)
        {
            int utf8Length = WideCharToMultiByte(
                CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
            if (utf8Length > 0)
            {
                std::string utf8Arg;
                // Exclued null-terminator
                utf8Arg.resize(utf8Length - 1);
                WideCharToMultiByte(
                    CP_UTF8, 0, argv[i], -1, &utf8Arg[0], utf8Length, nullptr, nullptr);
                args.push_back(utf8Arg);
            }
        }
        LocalFree(argv);
    #elif defined(__APPLE__)
        args = nx::kit::apple_utils::getProcessCmdLineArgs();
    #else //< Assuming Linux-like OS.
        std::ifstream inputStream("/proc/self/cmdline");
        std::string arg;
        while (std::getline(inputStream, arg, '\0'))
            args.push_back(arg);
    #endif

    // Ensure at least one element in the return value since we rely on the fact that this
    // function returns non-empty vector.
    if (args.empty())
        args.push_back("");

    return args;
}

bool fileExists(const char* filename)
{
    return static_cast<bool>(std::ifstream(filename));
}

/**
 * Appends an error message to std::string* errorMessage, concatenating via a space if
 * *errorMessage was not empty. Does nothing if errorMessage is null.
 *
 * NOTE: This is a macro to avoid composing the error message if errorMessage is null.
 */
#define ADD_ERROR_MESSAGE(ERROR_MESSAGE) do \
{ \
    if (errorMessage) \
    { \
        if (!errorMessage->empty()) \
            *errorMessage += " "; \
        *errorMessage += (ERROR_MESSAGE); \
    } \
} while (0)

/**
 * Parses the octal escape sequence at the given position.
 * @param p Pointer to a position right after the backslash; must contain an octal digit. After the
 *     call, the pointer is moved to the position after the escape sequence.
 * @param errorMessage A string which the error message, if any, will be appended to, if not null.
 * @return Whether the given position represents an octal escape sequence.
 */
static std::string decodeOctalEscapeSequence(const char** pp, std::string* errorMessage)
{
    // According to the C and C++ standards, up to three octal digits are recognized.
    // If the resulting number is greater than 255, it has been decided to consider it an error
    // rather than a Unicode character (the standard leaves it to the implementation).

    int code = 0;
    int digitCount = 0;
    while (**pp >= '0' && **pp <= '7')
    {
        ++digitCount;
        if (digitCount > 3)
            break;
        code = (code << 3) + (**pp - '0');
        ++(*pp);
    }

    if (code > 255 && errorMessage)
    {
        ADD_ERROR_MESSAGE(nx::kit::utils::format(
            "Octal escape sequence does not fit in one byte: %d.", code));
    }

    return std::string(1, (char) code);
}

/**
 * @return -1 if the char is not a hex digit.
 */
static int toHexDigit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/**
 * Parses the "\x" escape sequence.
 * @param p Pointer to a position right after "\x". After the call, the pointer is moved to the
 *     position after the escape sequence.
 * @param errorMessage A string which the error message, if any, will be appended to, if not null.
 */
static std::string decodeHexEscapeSequence(const char** pp, std::string* errorMessage)
{
    // According to the C and C++ standards, as much hex digits are part of the sequence as can be
    // found, and if the resulting number does not fit into char, it's an implementation decision -
    // we have decided to produce an error rather than a Unicode character.

    uint8_t code = 0;
    bool fitsInByte = true;

    int hexDigit = toHexDigit(**pp);

    if (hexDigit < 0)
    {
        ADD_ERROR_MESSAGE("Missing hex digits in the hex escape sequence.");
        return "";
    }

    while (hexDigit >= 0)
    {
        if (code > 0x0F)
            fitsInByte = false;
        code = (code << 4) + (uint8_t) hexDigit;
        ++(*pp);
        hexDigit = toHexDigit(**pp);
    }

    if (!fitsInByte)
        ADD_ERROR_MESSAGE("Hex escape sequence does not fit in one byte.");

    return std::string(1, (char) code);
}

/**
 * Parses the "\x" escape sequence.
 * @param p Pointer to a position right after "\". After the call, the pointer is moved to the
 *     position after the escape sequence.
 * @param errorMessage A string which the error message, if any, will be appended to, if not null.
 */
static std::string decodeEscapeSequence(const char** pp, std::string* errorMessage)
{
    const char escaped = **pp;

    if (escaped >= '0' && escaped <= '7')
        return decodeOctalEscapeSequence(pp, errorMessage);
    ++(*pp); //< Now points after the first char after the backslash.

    switch (escaped)
    {
        case '\'': case '\"': case '\?': case '\\': return std::string(1, escaped);
        case 'a': return "\a";
        case 'b': return "\b";
        case 'f': return "\f";
        case 'n': return "\n";
        case 'r': return "\r";
        case 't': return "\t";
        case 'v': return "\v";
        case 'x': return decodeHexEscapeSequence(pp, errorMessage);
        // NOTE: Unicode escape sequences `\u` and `\U` are not supported because they
        // would require generating UTF-8 sequences.
        case '\0':
            --(*pp); //< Move back to point to '\0'.
            ADD_ERROR_MESSAGE("Missing escaped character after the backslash.");
            return "\\";
        default:
            ADD_ERROR_MESSAGE(
                "Invalid escaped character " + toString(escaped) + " after the backslash.");
            return std::string(1, escaped);
    }
}

NX_KIT_API std::string decodeEscapedString(const std::string& s, std::string* errorMessage)
{
    if (errorMessage)
        *errorMessage = "";

    std::string result;
    result.reserve(s.size()); //< Most likely, the result will have almost the size of the source.

    const char* p = s.c_str();
    if (*p != '"')
    {
        ADD_ERROR_MESSAGE("The string does not start with a quote.");
        return s; //< Return the input string as-is.
    }
    ++p;

    for (;;)
    {
        const char c = *p;
        switch (c)
        {
            case '\0':
                ADD_ERROR_MESSAGE("Missing the closing quote.");
                return result;
            case '"': //< Support C-style concatenation of consecutive enquoted strings.
                ++p;
                while (*p > 0 && *p <= 32) //< Skip all whitespace between the quotes.
                    ++p;
                if (*p == '\0')
                    return result;
                if (*p != '"')
                {
                    ADD_ERROR_MESSAGE("Unexpected trailing after the closing quote.");
                    result += p; //< Append the unexpected trailing to the result as is.
                    return result;
                }
                ++p; //< Skip the quote.
                break;
            case '\\':
            {
                ++p;
                result += decodeEscapeSequence(&p, errorMessage);
                break;
            }
            default:
                if (/*allow non-ASCII characters*/ (unsigned char) c < 128 && !isAsciiPrintable(c))
                    ADD_ERROR_MESSAGE("Found non-printable ASCII character " + toString(c) + ".");
                result += c;
                ++p;
                break;
        }
    }
}

template<int sizeOfChar> struct HexEscapeFmt {};
template<> struct HexEscapeFmt<1> { static constexpr const char* value = "\\x%02X"; };
template<> struct HexEscapeFmt<2> { static constexpr const char* value = "\\u%04X"; };
template<> struct HexEscapeFmt<4> { static constexpr const char* value = "\\U%08X"; };

template<typename Char>
static std::string hexEscapeFmtForChar()
{
    return HexEscapeFmt<sizeof(Char)>::value;
}

template<typename Char>
static std::string hexEscapeFmtForString()
{
    std::string value = HexEscapeFmt<sizeof(Char)>::value;

    // Empty quotes after `\x` are needed to limit the hex sequence if a digit follows it.
    if (value[0] == '\\' && value[1] == 'x')
        return value + "\"\"";

    return value;
}

template<typename Char>
using Unsigned = typename std::make_unsigned<Char>::type;

template<typename Char>
static std::string toStringFromChar(Char c)
{
    // NOTE: If the char is not a printable ASCII, we escape it via `\x` instead of specialized
    // escape sequences like `\r` because it looks more clear and universal.
    if (!isAsciiPrintable(c))
        return format("'" + hexEscapeFmtForChar<Char>() + "'", (Unsigned<Char>) c);
    if (c == '\'')
        return "'\\''";

    return std::string("'") + (char) c + "'";
}

template<typename Char>
static std::string escapeCharInString(Char c)
{
    switch (c)
    {
        case '\0': return "\\000"; // Three octal digits are needed to limit the octal sequence.
        case '\\': case '"': return std::string("\\") + (char) c;
        case '\n': return "\\n";
        case '\r': return "\\r";
        case '\t': return "\\t";
        // NOTE: Escape sequences `\a`, `\b`, `\f`, `\v` are not generated above: they are
        // rarely used, and their representation via `\x` is more clear.
        default:
            if (!isAsciiPrintable(c))
                return format(hexEscapeFmtForString<Char>(), (Unsigned<Char>) c);
            return std::string(1, (char) c);
    }
}

template<typename Char>
std::string toStringFromPtr(const Char* s)
{
    std::string result;
    if (s == nullptr)
    {
        result = "null";
    }
    else
    {
        result = "\"";
        for (const Char* p = s; *p != '\0'; ++p)
            result += escapeCharInString(*p);
        result += "\"";
    }
    return result;
}

std::string toString(const std::string& s)
{
    std::string result = "\"";
    for (char c: s)
        result += escapeCharInString(c);
    result += "\"";
    return result;
}

std::string toString(const std::wstring& w)
{
    std::string result = "\"";
    for (wchar_t c: w)
        result += escapeCharInString(c);
    result += "\"";
    return result;
}

std::string toString(bool b)
{
    return b ? "true" : "false";
}

std::string toString(const void* ptr)
{
    return ptr ? format("%p", ptr) : "null";
}

std::string toString(char c)
{
    return toStringFromChar(c);
}

std::string toString(const char* s)
{
    return toStringFromPtr(s);
}

std::string toString(wchar_t w)
{
    return toStringFromChar(w);
}

std::string toString(const wchar_t* w)
{
    return toStringFromPtr(w);
}

bool fromString(const std::string& s, int* value)
{
    if (!value || s.empty())
        return false;

    // NOTE: std::stoi() is missing on Android, thus, using std::strtol().
    char* pEnd = nullptr;
    errno = 0; //< Required before std::strtol().
    const long v = std::strtol(s.c_str(), &pEnd, /*base*/ 0);

    if (v > INT_MAX || v < INT_MIN || errno != 0 || *pEnd != '\0')
        return false;

    *value = (int) v;
    return true;
}

template<typename Value>
bool fromStringViaStringStream(const std::string& s, Value* outValue)
{
    if (!outValue || s.empty())
        return false;

    Value result{};
    std::istringstream stream(s);
    stream.imbue(std::locale("C"));
    stream >> result;

    const bool success = stream.eof() && !stream.bad();
    if (success)
        *outValue = result;

    return success;
}

bool fromString(const std::string& s, double* outValue)
{
    return fromStringViaStringStream(s, outValue);
}

bool fromString(const std::string& s, float* outValue)
{
    return fromStringViaStringStream(s, outValue);
}

bool fromString(const std::string& s, bool* value)
{
    if (s == "true" || s == "True" || s == "TRUE" || s == "1")
        *value = true;
    else if (s == "false" || s == "False" || s == "FALSE" || s == "0")
        *value = false;
    else
        return false;
    return true;
}

void stringReplaceAllChars(std::string* s, char sample, char replacement)
{
    std::transform(s->cbegin(), s->cend(), s->begin(),
        [=](char c) { return c == sample ? replacement : c; });
}

void stringInsertAfterEach(std::string* s, char sample, const char* const insertion)
{
    for (int i = (int) s->size() - 1; i >= 0; --i)
    {
        if ((*s)[i] == sample)
            s->insert((size_t) (i + 1), insertion);
    }
}

void stringReplaceAll(std::string* s, const std::string& sample, const std::string& replacement)
{
    size_t pos = 0;
    while ((pos = s->find(sample, pos)) != std::string::npos)
    {
         s->replace(pos, sample.size(), replacement);
         pos += replacement.size();
    }
}

bool stringStartsWith(const std::string& s, const std::string& prefix)
{
    return s.rfind(prefix, /*pos*/ 0) == 0;
}

bool stringEndsWith(const std::string& s, const std::string& suffix)
{
    return s.size() >= suffix.size()
        && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string trimString(const std::string& s)
{
    int start = 0;
    while (start < (int) s.size() && s.at(start) <= ' ')
        ++start;
    int end = (int) s.size() - 1;
    while (end >= 0 && s.at(end) <= ' ')
        --end;
    if (end < start)
        return "";
    return s.substr(start, end - start + 1);
}

//-------------------------------------------------------------------------------------------------
// Parsing utils.

namespace {

static void skipWhitespace(const char** const pp)
{
    while (**pp != '\0' && isSpaceOrControlChar(**pp))
        ++(*pp);
}

struct ParsedNameValue
{
    std::string name; //< Empty if the line must be ignored.
    std::string value;
    std::string error; //< Empty on success.
};

static ParsedNameValue parseNameValue(const std::string& lineStr)
{
    ParsedNameValue result;

    const char* p = lineStr.c_str();

    skipWhitespace(&p);
    if (*p == '\0' || *p == '#') //< Empty or comment line.
        return result;
    while (*p != '\0' && *p != '=' && !isSpaceOrControlChar(*p))
        result.name += *(p++);
    if (result.name.empty())
    {
        result.error = "The name part (before \"=\") is empty.";
        return result;
    }
    skipWhitespace(&p);

    if (*(p++) != '=')
    {
        result.error = "Missing \"=\" after the name " + toString(result.name) + ".";
        return result;
    }
    skipWhitespace(&p);

    while (*p != '\0')
        result.value += *(p++);

    // Trim trailing whitespace in the value.
    int i = (int) result.value.size() - 1;
    while (i >= 0 && isSpaceOrControlChar(result.value[i]))
        --i;
    result.value = result.value.substr(0, i + 1);

    return result;
}

} // namespace

bool parseNameValueFile(
    const std::string& nameValueFilePath,
    std::map<std::string, std::string>* nameValueMap,
    const std::string& errorPrefix,
    std::ostream* output,
    bool* isFileEmpty)
{
    nameValueMap->clear();

    std::ifstream file(nameValueFilePath);
    if (!file.good())
        return false;

    bool result = true;

    std::string lineStr;
    int line = 0;
    while (std::getline(file, lineStr))
    {
        ++line;

        static constexpr char kBom[] = "\xEF\xBB\xBF";
        if (line == 1 && stringStartsWith(lineStr, kBom))
            lineStr.erase(0, sizeof(kBom) - /* Terminating NUL */ 1);
        
        const ParsedNameValue parsed = parseNameValue(lineStr);
        if (!parsed.error.empty())
        {
            if (output)
            {
                *output << errorPrefix << "ERROR: " << parsed.error << " Line " << line
                    << ", file " << nameValueFilePath << std::endl;
            }
            result = false;
            continue;
        }

        if (!parsed.name.empty())
            (*nameValueMap)[parsed.name] = parsed.value;
    }

    if (isFileEmpty != nullptr)
        *isFileEmpty = line == 0;
    return result;
}

std::string toUpper(const std::string& str)
{
    std::string result = str;
    for (auto& c: result)
        c = (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
    return result;
}

} // namespace utils
} // namespace kit
} // namespace nx
