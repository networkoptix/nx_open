// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <array>
#include <string>
#include <vector>
#include <locale>
#include <iostream>

#include <Windows.h>
#include <Shlwapi.h>
#include <shlobj.h>
#include <shellapi.h>

#include "version.h"

namespace {

static constexpr auto kQuote = L'\"';
static constexpr auto kSpace = L' ';
static constexpr auto kSlash = L'/';
static constexpr auto kBackSlash = L'\\';

// Appends unix-style slashe to the end of the directory name if needed.
std::wstring closeDirPath(const std::wstring& name)
{
    if (name.empty())
        return name;

    if (name[name.length() - 1] == kSlash || name[name.length() - 1] == kBackSlash)
        return name;

    return name + kSlash;
}

// Replaces windows-style backslashes to unix-style slashes.
std::wstring toStandardPath(const std::wstring& path)
{
    std::wstring result = path;
    for (auto& symbol: result)
    {
        if (symbol == kBackSlash)
            symbol = kSlash;
    }
    return result;
}

std::wstring getSharedApplauncherDir()
{
    wchar_t result[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, result)))
    {
        // Append product-specific path
        PathAppend(result, L"\\" QN_ORGANIZATION_NAME  L"\\applauncher\\"  QN_CUSTOMIZATION_NAME);
    }
    return std::wstring(closeDirPath(result));
}

std::wstring getInstalledApplauncherDir()
{
    wchar_t ownPath[MAX_PATH];

    // Will contain exe path.
    const HMODULE hModule = GetModuleHandle(nullptr);
    if (hModule != nullptr)
    {
        // When passing NULL to GetModuleHandle, it returns handle of exe itself.
        GetModuleFileNameW(hModule, ownPath, MAX_PATH);
        PathRemoveFileSpecW(ownPath);
    }

    return std::wstring(closeDirPath(ownPath));
}

std::wstring getFullFileName(const std::wstring& folder, const std::wstring& fileName)
{
    return closeDirPath(toStandardPath(folder)) + fileName;
}

bool startProcessAsync(
    const std::wstring& applicationPath,
    std::wstring commandLine,
    const std::wstring& currentDirectory)
{
    STARTUPINFO lpStartupInfo;
    PROCESS_INFORMATION lpProcessInfo;
    memset(&lpStartupInfo, 0, sizeof(lpStartupInfo));
    memset(&lpProcessInfo, 0, sizeof(lpProcessInfo));

    return CreateProcess(
        applicationPath.c_str(),
        &commandLine[0],
        /*lpProcessAttributes*/ nullptr,
        /*lpThreadAttributes*/ nullptr,
        /*bInheritHandles*/ false,
        /*dwCreationFlags*/ 0,
        /*lpEnvironment*/ nullptr,
        currentDirectory.c_str(),
        &lpStartupInfo,
        &lpProcessInfo
    );
}

bool launchInDir(const std::wstring& currentDirectory, int argc, LPWSTR* argv)
{
    std::wcout << L"Launching in path " << currentDirectory << std::endl;
    const auto applicationPath = getFullFileName(currentDirectory, QN_APPLAUNCHER_BINARY_NAME);
    auto commandLine = kQuote + applicationPath + kQuote;

    for (int i = 1; i < argc; ++i)
    {
        std::wstring argument(argv[i]);
        const auto quotedArgument = kQuote + argument + kQuote;
        commandLine += kSpace + quotedArgument;
    }

    std::wcout << L"Command line: " << commandLine.c_str() << std::endl;

    try
    {
        return startProcessAsync(applicationPath, commandLine, currentDirectory);
    }
    catch (...)
    {
        return false;
    }
}

using Version = std::array<int, 4>;

const static Version kMinilauncherVersion = {
    QN_APP_VERSION_MAJOR,
    QN_APP_VERSION_MINOR,
    QN_APP_VERSION_INCREMENTAL,
    QN_APP_VERSION_BUILD_NUMBER};

Version getSharedApplauncherVersion()
{
    Version result = {0, 0, 0, 0};

    const auto applauncherPath = getFullFileName(
        getSharedApplauncherDir(),
        QN_APPLAUNCHER_VERSION_FILE);

    auto versionFile = _wfopen(applauncherPath.c_str(), L"r");
    if (versionFile)
    {
        fscanf(versionFile, "%d.%d.%d.%d", &result[0], &result[1], &result[2], &result[3]);
        fclose(versionFile);
    }

    return result;
}

int launchFile(int argc, LPWSTR* argv)
{
    // In some rare cases existing applauncher may fail to launch the just installed client binary
    // (e.g. when client installation path was changed during rebranding).
    // We should use local applauncher to make client execution possible. Shared applauncher and
    // existing minilauncher copies will be updated by the executed client, so after the first
    // client launch shared applauncher and all minilaunchers will have the same (new) version.
    if (getSharedApplauncherVersion() < kMinilauncherVersion)
    {
        // The shared applauncher is outdated or does not exist.
        if (launchInDir(getInstalledApplauncherDir(), argc, argv))
            return 0;
    }
    else
    {
        // Shared applauncher has been updated already.
        if (launchInDir(getSharedApplauncherDir(), argc, argv))
            return 0;
    }

    return 1;
}

} // namespace

int main()
{
    try
    {
        int argc{};
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

        setlocale(LC_CTYPE, ""); //< For correct std::wcout work.

        /* All additional arguments are passed to applauncher. */
        return launchFile(argc, argv);
    }
    catch (...)
    {
        return 2;
    }
}
