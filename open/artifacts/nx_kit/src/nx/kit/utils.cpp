#include "utils.h"

#include <vector>
#include <string>
#include <algorithm>
#include <climits>
#include <cerrno>

#if defined(_WIN32)
    #define NOMINMAX //< Needed to prevent windows.h define macros min() and max().
    #include <windows.h>
    #include <codecvt>
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
            args = std::vector<std::string>{""};
            return args;
        }

        for (int i = 0; i < argc; ++i)
        {
            args.push_back(
                std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(argv[i]));
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

    return args;
}

std::string toString(std::string s)
{
    return toString(s.c_str());
}

std::string toString(uint8_t i)
{
    return toString((int) i);
}

std::string toString(char c)
{
    if (!isAsciiPrintable(c))
        return format("'\\x%02X'", (unsigned char) c);
    if (c == '\'')
        return "'\\''";
    return std::string("'") + c + "'";
}

std::string toString(const char* s)
{
    std::string result;
    if (s == nullptr)
    {
        result = "null";
    }
    else
    {
        result = "\"";
        for (const char* p = s; *p != '\0'; ++p)
        {
            if (*p == '\\' || *p == '"')
                (result += '\\') += *p;
            else if (*p == '\n')
                result += "\\n";
            else if (*p == '\t')
                result += "\\t";
            else if (!isAsciiPrintable(*p))
                result += format("\\x%02X", (unsigned char) *p);
            else
                result += *p;
        }
        result += "\"";
    }
    return result;
}

std::string toString(char* s)
{
    return toString(const_cast<const char*>(s));
}

std::string toString(const void* ptr)
{
    return ptr ? format("%p", ptr) : "null";
}

std::string toString(void* ptr)
{
    return toString(const_cast<const void*>(ptr));
}

std::string toString(std::nullptr_t ptr)
{
    return toString((const void*) ptr);
}

std::string toString(bool b)
{
    return b ? "true" : "false";
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

bool fromString(const std::string& s, double* value)
{
    if (!value || s.empty())
        return false;

    // NOTE: std::stod() is missing on Android, thus, using std::strtod().
    char* pEnd = nullptr;
    errno = 0; //< Required before std::strtod().
    const double v = std::strtod(s.c_str(), &pEnd);

    if (errno == ERANGE || *pEnd != '\0')
        return false;

    *value = v;
    return true;
}

bool fromString(const std::string& s, float* value)
{
    if (!value || s.empty())
        return false;

    // NOTE: std::stof() and std::strtof() are missing on Android, thus, using std::strtod().
    char* pEnd = nullptr;
    errno = 0; //< Required before std::strtod().
    const float v = (float) std::strtod(s.c_str(), &pEnd);
    if (errno == ERANGE || *pEnd != '\0')
        return false;

    *value = (float) v;
    return true;
}

} // namespace utils
} // namespace kit
} // namespace nx
