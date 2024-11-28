// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#if defined(__linux__) || defined(__APPLE__)
#   include <sys/stat.h>
#endif

#include "ftp_library.h"

#ifdef _MSC_VER
#   define NOEXCEPT
#elif defined __GNUC__
#   define NOEXCEPT noexcept
#endif

namespace nx_spl {
namespace aux {

// Scoped file remover
class FileRemover
{
public:
    FileRemover(const std::string& fname) :
        m_fname(fname)
    {}

    ~FileRemover()
    {
        std::remove(m_fname.c_str());
    }
private:
    std::string m_fname;
};

// Scoped file closer
class FileCloser
{
public:
    FileCloser(FILE *f, int *ecode, error::code_t errToSet = error::UnknownError)
        : m_f(f),
            m_ecode(ecode),
            m_err(errToSet)
    {}

    ~FileCloser()
    {
        if (m_f)
            fclose(m_f);
        if (m_ecode)
            *m_ecode = m_err;
    }
private:
    FILE* m_f;
    int* m_ecode;
    error::code_t m_err;
};

std::string urlDecode(std::string s)
{
    static const std::unordered_map<std::string, std::string> kUrlDecodeTable = {
        {"%20", " "}, {"%21", "!"}, {"%22", "\""}, {"%23", "#"}, {"%24", "$"}, {"%25", "%"},
        {"%26", "&"}, {"%27", "'"}, {"%28", "("}, {"%29", ")"}, {"%2A", "*"}, {"%2B", "+"},
        {"%2C", ","}, {"%2D", "-"}, {"%2E", "."}, {"%2F", "/"}, {"%3A", ":"}, {"%3B", ";"},
        {"%3C", "<"}, {"%3D", "="}, {"%3E", ">"}, {"%3F", "?"}, {"%40", "@"}, {"%5B", "["},
        {"%5C", "\\"}, {"%5D", "]"}, {"%5E", "^"}, {"%5F", "_"}, {"%60", "`"}, {"%7B", "{"},
        {"%7C", "|"}, {"%7D", "}"}, {"%7E", "~"}
    };

    std::string result;
    for (size_t i = 0; i < s.size();)
    {
        if (s[i] == '%' && i < s.size() - 2)
        {
            const auto it = kUrlDecodeTable.find(std::string(s.cbegin() + i, s.cbegin() + i + 3));
            if (it != kUrlDecodeTable.cend())
            {
                result += it->second;
                i += 3;
                continue;
            }
        }

        result.push_back(s[i++]);
    }

    return result;
}

// Url data structure and custom parsing function
struct Url
{
    std::string uname;
    std::string upasswd;
    std::string host;
    std::string port;
    std::string path;

    static Url fromString(const std::string& s)
    {
        enum
        {   // parse states
            scheme,
            user,
            pwd,
            host,
            port
        } ps = scheme;

        const int schemeSize = 6; // "ftp://" size
        int start = 0, cur = 0;
        char c;
        Url u;

        if (s.size() <= schemeSize)
            throw std::logic_error("Url parse failed. Too short.");

        for (;;)
        {
            switch (ps)
            {
            case scheme:
                if (s.substr(0, schemeSize) != "ftp://")
                    throw std::logic_error("Url parse failed. Wrong scheme.");
                start = schemeSize;
                cur = schemeSize;
                // there can be no username substring in url. It's ok.
                if (s.find('@', schemeSize) != std::string::npos)
                    ps = user;
                else
                    ps = host;
                break;
            case user:
                if (cur == (int) s.size()) // That's all? No, url can't end here
                    throw std::logic_error("Url parse failed. Url string ended at username parse stage");
                c = s[cur];
                if (c != ':' && c != '@')
                    ++cur;
                else if (c == ':')
                {
                    if (cur - start == 0) // if you wrote ':', you should have provided some non-empty name before.
                        throw std::logic_error("Url parse failed. Username is empty");
                    u.uname.assign(s.begin() + start, s.begin() + cur);
                    ++cur;
                    start = cur;
                    ps = pwd;
                }
                else if (c == '@')
                {
                    if (cur - start == 0) // if you wrote '@', you should have provided some non-empty name before.
                        throw std::logic_error("Url parse failed. Username is empty");
                    u.uname.assign(s.begin() + start, s.begin() + cur);
                    ++cur;
                    start = cur;
                    ps = host;
                }
                break;
            case pwd:
                if (cur == (int) s.size()) // That's all? No, url can't end here
                    throw std::logic_error("Url parse failed. Url string ended at password parse stage");
                c = s[cur];
                if (c != '@')
                    ++cur;
                else
                {
                    u.upasswd.assign(s.begin() + start, s.begin() + cur);
                    ++cur;
                    start = cur;
                    ps = host;
                }
                break;
            case host:
                if (cur == (int) s.size())
                {
                    u.host.assign(s.begin() + start, s.begin() + cur);
                    goto end;
                }
                c = s[cur];
                if (c == '/') //path begins
                {
                    u.host.assign(s.begin() + start, s.begin() + cur);
                    u.path.assign(s.begin() + cur, s.end());
                    goto end;
                }

                if (c != ':')
                    ++cur;
                else
                {
                    if (cur - start == 0) // That's all? No, url can't end here
                        throw std::logic_error("Url parse failed. Host is empty");
                    u.host.assign(s.begin() + start, s.begin() + cur);
                    ++cur;
                    start = cur;
                    ps = port;
                }
                break;
            case port:
                c = s[cur];
                if (c == '/') //path begins
                {
                    u.port.assign(s.begin() + start, s.begin() + cur);
                    u.path.assign(s.begin() + cur, s.end());
                    goto end;
                }

                if (cur == (int) s.size())
                {
                    if (cur - start == 0) // If you wrote ':' after hostname, provide some valid port value too
                        throw std::logic_error("Url parse failed. Port is empty");
                    u.port.assign(s.begin() + start, s.begin() + cur);
                    goto end;
                }
                if (!std::isdigit(s[cur]))
                    throw std::logic_error("Url parse failed. Port should contain digits only");
                ++cur;
                break;
            default:
                // something strange happened if we are here
                throw std::logic_error("Url parse failed. Invalid parse state.");
            }
        }

    end:
        u.upasswd = urlDecode(u.upasswd);
        return u;
    } // Url::fromString()
}; // struct Url

namespace test
{ // This is some basic url parsing unit tests
    // These functions are not called anywhere in this implementation,
    // but left here in case Url parser implementation will be altered and some regression sanity is needed.

#define REQUIRE(expr) do {if (!(expr)) {std::printf(#expr" failed. LINE: %d\n\n", __LINE__); return;}} while(0)

#define REQUIRE_THROW(expr) \
do \
{ \
    try { (expr); {std::printf(#expr" THROW failed. LINE: %d\n\n", __LINE__); return;} } catch(...) {} \
} \
while(0)

    void urlParserTest()
    {
        REQUIRE_THROW(Url::fromString("ftp://:pupkin@127.0.0.1:765"));
        //REQUIRE_THROW(Url::fromString("ftp://vasya:@127.0.0.1:765"));
        REQUIRE_THROW(Url::fromString("fp://vasya:pupkin@127.0.0.1:765"));
        REQUIRE_THROW(Url::fromString("ftp://vasya:pupkin@127.0.0.1:a765"));
        REQUIRE_THROW(Url::fromString("ftp://@127.0.0.1:765"));
        REQUIRE_THROW(Url::fromString("ftp://:127.0.0.1:765"));
        REQUIRE_THROW(Url::fromString("ftp://"));
        REQUIRE_THROW(Url::fromString("127.0.0.1"));
        REQUIRE_THROW(Url::fromString("127.0.0.1:567"));

        Url u ;
        u = Url::fromString("ftp://vasya:pupkin@127.0.0.1:765");
        REQUIRE(u.host == "127.0.0.1");
        REQUIRE(u.port == "765");
        REQUIRE(u.uname == "vasya");
        REQUIRE(u.upasswd == "pupkin");

        u = Url::fromString("ftp://vasya:pupkin@127.0.0.1");
        REQUIRE(u.host == "127.0.0.1");
        REQUIRE(u.port.empty());
        REQUIRE(u.uname == "vasya");
        REQUIRE(u.upasswd == "pupkin");

        u = Url::fromString("ftp://127.0.0.1:765");
        REQUIRE(u.host == "127.0.0.1");
        REQUIRE(u.port == "765");
        REQUIRE(u.uname.empty());
        REQUIRE(u.upasswd.empty());

        u = Url::fromString("ftp://127.0.0.1");
        REQUIRE(u.host == "127.0.0.1");
        REQUIRE(u.port.empty());
        REQUIRE(u.uname.empty());
        REQUIRE(u.upasswd.empty());
    } // urlParseTest
#undef REQUIRE
#undef REQUIRE_THROW
} // namespace test

std::optional<std::vector<std::string>> readLines(const std::string& path)
{
    std::ifstream ifs(path);
    if (!ifs)
        return std::nullopt;

    std::string line;
    std::vector<std::string> result;
    while (!ifs.eof())
    {
        std::getline(ifs, line);
        if (!line.empty())
            result.push_back(line);
    }

    return result;
}

// Cut dir name from file name and return both.
void dirFromUri(
    const std::string   &uri, // In
    std::string         *dir, // Out. directory name
    std::string         *file // Out. file name
)
{
    std::string::size_type pos;
    if ((pos = uri.rfind('/')) == std::string::npos)
    {   // Here we think that '/' is the only path separator.
        // Maybe generally it is not very reliable.
        *dir = ".";
        *file = uri;
    }
    else
    {
        dir->assign(uri.begin(), uri.begin() + pos);
        if (!dir->empty() && dir->at(dir->size() - 1) != '/')
            *dir += '/';
        if (pos < uri.size() - 1) // if file name is not empty
            file->assign(uri.begin() + pos + 1, uri.end());
    }
}

static FileNameAndPath localUniqueFilePath(const std::string& suffix = std::string());

std::string toRelativeFtpPath(std::string path)
{
    if (path.empty())
        return "./";

    if (path.size() >= 2 && path[0] == '.' && path[1] == '/')
        return path;

    return path[0] == '/' ? "." + path : "./" + path;
}

// get local file size by it's name
long long getFileSize(const char *fname)
{
#ifdef _WIN32

    HANDLE hFile = CreateFileA(
        fname,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (hFile == INVALID_HANDLE_VALUE)
        return -1;

    LARGE_INTEGER s;
    if (!GetFileSizeEx(hFile, &s))
    {
        CloseHandle(hFile);
        return -1;
    }
    CloseHandle(hFile);
    return s.QuadPart;

#elif defined(__linux__) || defined(__APPLE__)

    struct stat st;
    if (stat(fname, &st) == -1)
        return -1;
    return st.st_size;
#endif
}

void log(const std::string& message)
{
    static bool kLoggingEnabled = false;
    if (kLoggingEnabled)
        std::cout << "ftp storage: " << message << std::endl;
}

class FtpLibWrapper
{
public:
    FtpLibWrapper(const std::string& url);
    bool isAvailable() const;
    std::optional<std::vector<std::string>> dir(const std::string& path) const;
    bool put(const std::string& localPath, const std::string& remotePath);
    bool get(const std::string& localPath, const std::string& remotePath);
    bool removeFile(const std::string& remotePath);
    bool removeDir(const std::string& remotePath);
    bool renameFile(const std::string& oldPath, const std::string& newPath);
    std::optional<std::vector<std::string>> nlst(const std::string& remotePath) const;
    bool pathExists(const std::string& remotePath) const;
    std::optional<int> fileSize(const std::string& remotePath) const;
    bool makePath(const std::string& dirPath);
    std::optional<FileType> fileType(const std::string& remotePath);

private:
    void establishFtpConnection();
    void ftpReconnect() const;

    template<typename F>
    bool executeWithRetry(F f, const std::string& command) const;

private:
    std::string m_url;
    std::string m_username;
    std::string m_password;
    mutable std::mutex m_mutex;
    mutable std::unique_ptr<ftplib> m_ftplib;
};

void FtpLibWrapper::ftpReconnect() const
{
    m_ftplib = std::make_unique<ftplib>();
    if (m_ftplib->Connect(m_url.data()) == 0)
        throw std::runtime_error("Couldn't connect");

    if (m_ftplib->Login(m_username.data(), m_password.data()) == 0)
        throw std::runtime_error("Couldn't login");
}

FtpLibWrapper::FtpLibWrapper(const std::string& url)
{
    const auto u = aux::Url::fromString(url);
    m_url = u.host + ':' + (u.port.empty() ? std::string("21") : u.port);
    m_username = u.uname.empty() ? "anonymous" : u.uname.data();
    m_password = u.upasswd.empty() ? "" : u.upasswd.data();
    ftpReconnect();
}

template<typename F>
bool FtpLibWrapper::executeWithRetry(F f, const std::string& command) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (f())
    {
        log(command + " succeeded");
        return true;
    }

    try
    {
        log(command + " failed. Retrying.");
        ftpReconnect();
        if (!f())
        {
            log(command + " retry failed");
            return false;
        }
    }
    catch(...)
    {
        log(command + " retry failed.");
        return false;
    }

    return true;
}

#define EXEC_WITH_RETRY(command, args) \
    executeWithRetry([&]() { return m_ftplib->command args; }, #command)

bool FtpLibWrapper::isAvailable() const
{
    return EXEC_WITH_RETRY(TestControlConnection, ());
}

std::optional<std::vector<std::string>> FtpLibWrapper::dir(const std::string& path) const
{
    const auto fileInfo = aux::localUniqueFilePath();
    aux::FileRemover fr(fileInfo.fullPath);
    log("Dir. path: '" + path + "'");
    if (!EXEC_WITH_RETRY(Dir, (fileInfo.fullPath.data(), path.data())))
        return std::nullopt;

    return readLines(fileInfo.fullPath);
}

bool FtpLibWrapper::put(const std::string& localPath, const std::string& remotePath)
{
    log("Put. path: '" + remotePath + "'");
    return EXEC_WITH_RETRY(Put, (localPath.data(), remotePath.data(), ftplib::image));
}

bool FtpLibWrapper::get(const std::string& localPath, const std::string& remotePath)
{
    log("Get. path: '" + remotePath + "'");
    return EXEC_WITH_RETRY(Get, (localPath.data(), remotePath.data(), ftplib::image));
}

bool FtpLibWrapper::removeFile(const std::string& remotePath)
{
    log("RemoveFile (Delete). path: '" + remotePath + "'");
    return EXEC_WITH_RETRY(Delete, (remotePath.data()));
}

bool FtpLibWrapper::removeDir(const std::string& remotePath)
{
    log("RmDir. path: '" + remotePath + "'");
    return EXEC_WITH_RETRY(Rmdir, (remotePath.data()));
}

bool FtpLibWrapper::renameFile(const std::string& oldPath, const std::string& newPath)
{
    log("Rename. old path: '" + oldPath + "', new path: '" + newPath + "'");
    return EXEC_WITH_RETRY(Rename, (oldPath.data(), newPath.data()));
}

std::optional<std::vector<std::string>> FtpLibWrapper::nlst(const std::string& remotePath) const
{
    const auto fileInfo = aux::localUniqueFilePath();
    aux::FileRemover fr(fileInfo.fullPath);

    log("nlst: arg: '" + aux::toRelativeFtpPath(remotePath) + "'");
    if (!EXEC_WITH_RETRY(Nlst, (fileInfo.fullPath.data(), aux::toRelativeFtpPath(remotePath).data())))
        return std::nullopt;

    const auto lines = readLines(fileInfo.fullPath);
    if (!lines)
    {
        log("nlst: Failed to read lines from a local file");
        return std::nullopt;
    }

    std::vector<std::string> result;
    for (auto line: *lines)
    {
        if (!line.empty())
        {
            log("    Nlst entry: '" + line + "'");
            result.push_back(line);
        }
    }

    return result;
}

bool FtpLibWrapper::pathExists(const std::string& remotePath) const
{
    const auto fileInfo = localUniqueFilePath();
    FileRemover fr(fileInfo.fullPath);
    std::string dir;
    std::string file;
    dirFromUri(remotePath, &dir, &file);

    if (file.empty()) // Uri shouldn't end with '/'
        throw std::runtime_error("empty file name");

    const auto result = nlst(dir);
    if (!result)
    {
        log("pathExists: nlst failed");
        return false;
    }

    const bool existsResult = std::any_of(
        result->cbegin(), result->cend(), [&remotePath](const auto& line) { return line == remotePath; });

    log("pathExists: result: " + std::to_string(existsResult));
    return existsResult;
}

std::optional<int> FtpLibWrapper::fileSize(const std::string& remotePath) const
{
    log("FileSize. path: '" + remotePath + "'");
    int size = -1;
    if (!EXEC_WITH_RETRY(Size, (remotePath.data(), &size, ftplib::image)))
        return std::nullopt;

    return size;
}

bool FtpLibWrapper::makePath(const std::string& dirPath)
{
    if (dirPath.empty() || dirPath[0] != '.' || dirPath[1] != '/')
    {
        assert(false);
        return false;
    }

    size_t pos = 2;
    while (pos < dirPath.size())
    {
        const size_t newPos = dirPath.find("/", pos);
        if (newPos == std::string::npos)
        {
            if (pathExists(dirPath))
                return true;

            log("Mkdir. (whole) path: '" + dirPath + "'");
            EXEC_WITH_RETRY(Mkdir, (dirPath.data()));
            break;
        }
        else
        {
            std::string path(dirPath.begin(), dirPath.begin() + newPos);
            if (!pathExists(path))
            {
                log("Mkdir. path: '" + path + "'");
                EXEC_WITH_RETRY(Mkdir, (path.data()));
            }

            pos = newPos + 1;
        }
    }

    return true;
}

std::optional<FileType> FtpLibWrapper::fileType(const std::string& remotePath)
{
    log("fileType. path: '" + remotePath +"'");
    if (const auto dirResult = dir(remotePath + "/"); dirResult && !dirResult->empty())
    {
        log("fileType. path: '" + remotePath + "' is Dir");
        return FileType::isDir;
    }

    if (pathExists(remotePath))
    {
        log("fileType. path: '" + remotePath + "' is File");
        return FileType::isFile;
    }

    log("fileType. path: '" + remotePath + "' is UNKNOWN");
    return std::nullopt;
}

/**
 * Generates a pseudo-random file name and a file path to the OS TMP directory + the generated name.
 * \param suffix If set it is appended to the result file name.
 */
static FileNameAndPath localUniqueFilePath(const std::string& suffix)
{
    /* First, get a system tmp path*/
    std::string tmpFolder;
    #if defined (_WIN32)
        char buf[MAX_PATH + 1];
        DWORD result = GetTempPathA(sizeof(buf), buf);
        assert(result > 0);
        if (result == 0)
            std::cerr << "Failed to get a temporary folder path" << std::endl;
        tmpFolder = buf;
    #elif defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
        for (const auto& v: {"TMP", "TEMP", "TMPDIR", "TEMPDIR"})
        {
            const char* envVar = getenv(v);
            if (envVar == nullptr)
                continue;
            tmpFolder = envVar;
            break;
        }
        if (tmpFolder.empty())
            tmpFolder = "/tmp";
    #else
        assert(false);
    #endif
    /* Now, when the base path is found, generate pseudo random bytes for a file name. */
    std::stringstream nameStream;
    for (int i = 0; i < 4; ++i)
        nameStream << std::hex << rand();
    /* Append the suffix, fill the result and we are done. */
    nameStream << suffix;
    FileNameAndPath nameAndPath;
    nameAndPath.name = nameStream.str();
    nameAndPath.fullPath = tmpFolder + "/" + nameAndPath.name;
    return nameAndPath;
}

} // namespace aux

// FileInfo Iterator
FtpFileInfoIterator::FtpFileInfoIterator(FtpImplPtr impl, FileListType&& fileList):
    m_impl(impl),
    m_fileList(std::move(fileList)),
    m_curFile(m_fileList.cbegin())
{
}

FtpFileInfoIterator::~FtpFileInfoIterator()
{}

FileInfo* FtpFileInfoIterator::next(int* ecode) const
{
    if (ecode)
        *ecode = nx_spl::error::NoError;

    if (m_curFile != m_fileList.cend())
    {
        m_fileInfo.url = m_curFile->data();
        const auto fileType = m_impl->fileType(aux::toRelativeFtpPath(*m_curFile));
        if (fileType)
            m_fileInfo.type = *fileType;

        ++m_curFile;
        return &m_fileInfo;
    }
    return nullptr;
}

void* FtpFileInfoIterator::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (std::memcmp(&interfaceID,
                    &IID_FileInfoIterator,
                    sizeof(nxpl::NX_GUID)) == 0)
    {
        addRef();
        return static_cast<nx_spl::FileInfoIterator*>(this);
    }
    else if (std::memcmp(&interfaceID,
                            &nxpl::IID_PluginInterface,
                            sizeof(nxpl::IID_PluginInterface)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

int FtpFileInfoIterator::addRef() const
{
    return p_addRef();
}

int FtpFileInfoIterator::releaseRef() const
{
    return p_releaseRef();
}

// Ftp Storage
FtpStorage::FtpStorage(const std::string& url)
{
    m_impl = std::make_shared<aux::FtpLibWrapper>(url);
}

FtpStorage::~FtpStorage()
{
}

int FtpStorage::isAvailable() const
{
    return m_impl->isAvailable();
}

uint64_t FtpStorage::getFreeSpace(int* ecode) const
{
    if (ecode)
        *ecode = error::NoError;

    return 1000LL * 1000 * 1000 * 100;
}

uint64_t FtpStorage::getTotalSpace(int* ecode) const
{
    if (ecode)
        *ecode = error::NoError;

    return 1000LL * 1000 * 1000 * 100;
}

int FtpStorage::getCapabilities() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_capabilities != 0)
        return m_capabilities;

    // list
    if (m_impl->dir("./"))
        m_capabilities |= cap::ListFile;

    const auto fileInfo = aux::localUniqueFilePath();
    aux::FileRemover fr(fileInfo.fullPath);

    FILE* ofs = fopen(fileInfo.fullPath.c_str(), "wb");
    if (ofs)
    {
        fwrite("1", 1, 2, ofs);
        fclose(ofs);
        if (m_impl->put(fileInfo.fullPath, fileInfo.name))
            m_capabilities |= cap::WriteFile;
    }
    else
        return m_capabilities;

    if (m_impl->get(fileInfo.fullPath.c_str(), fileInfo.name.c_str()))
        m_capabilities |= cap::ReadFile;

    // remove file
    if (m_impl->removeFile(fileInfo.name.c_str()))
        m_capabilities |= cap::RemoveFile;

    return m_capabilities;
}

void FtpStorage::removeFile(const char* url, int* ecode)
{
    *ecode = m_impl->removeFile(aux::toRelativeFtpPath(url))
        ? error::NoError : error::StorageUnavailable;
}

void FtpStorage::removeDir(const char* url, int *ecode)
{
    *ecode = m_impl->removeDir(aux::toRelativeFtpPath(url))
        ? error::NoError : error::StorageUnavailable;
}

void FtpStorage::renameFile(const char* oldUrl, const char* newUrl, int* ecode)
{
    *ecode = m_impl->renameFile(aux::toRelativeFtpPath(oldUrl), aux::toRelativeFtpPath(newUrl))
        ? error::NoError : error::StorageUnavailable;
}

FileInfoIterator* FtpStorage::getFileIterator(const char* dirUrl, int* ecode) const
{
    auto paths = m_impl->nlst(dirUrl);
    *ecode = paths ? error::NoError : error::StorageUnavailable;
    if (!paths)
        return nullptr;

    for (auto& p: *paths)
    {
        while (p.size() > 1 && (p[0] == '.' || p[0] == '/'))
            p = std::string(p.begin() + 1, p.end());
    }

    return new FtpFileInfoIterator(m_impl, std::move(*paths));
}

int FtpStorage::fileExists(const char* url, int* ecode) const
{
    *ecode = error::NoError;
    const auto remotePath = aux::toRelativeFtpPath(url);
    const auto type = m_impl->fileType(aux::toRelativeFtpPath(remotePath));
    if (type && *type == FileType::isFile)
        return 1;

    return 0;
}

int FtpStorage::dirExists(const char* url, int* ecode) const
{
    *ecode = error::NoError;
    const auto remotePath = aux::toRelativeFtpPath(url);
    const auto type = m_impl->fileType(aux::toRelativeFtpPath(remotePath));
    if (type && *type == FileType::isDir)
        return 1;

    return 0;
}

uint64_t FtpStorage::fileSize(const char* url, int* ecode ) const
{
    const auto result = m_impl->fileSize(aux::toRelativeFtpPath(url));
    if (!result)
    {
        *ecode = error::UnknownError;
        return unknown_size;
    }

    *ecode = error::NoError;
    return *result;
}

IODevice* FtpStorage::open(const char* uri, int flags, int* ecode) const
{
    *ecode = error::NoError;
    try
    {
        return new FtpIODevice(m_impl, uri, flags);
    }
    catch (...)
    {
        *ecode = error::UnknownError;
        return nullptr;
    }
}

void* FtpStorage::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (std::memcmp(&interfaceID, &IID_Storage, sizeof(nxpl::NX_GUID)) == 0)
    {
        addRef();
        return static_cast<nx_spl::Storage*>(this);
    }
    else if (std::memcmp(
        &interfaceID,
        &nxpl::IID_PluginInterface,
        sizeof(nxpl::IID_PluginInterface)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

int FtpStorage::addRef() const
{
    return p_addRef();
}

int FtpStorage::releaseRef() const
{
    return p_releaseRef();
}

// FtpStorageFactory
FtpStorageFactory::FtpStorageFactory()
{
    std::srand((unsigned int) time(0));
}

void* FtpStorageFactory::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (std::memcmp(&interfaceID,
                    &IID_StorageFactory,
                    sizeof(nxpl::NX_GUID)) == 0)
    {
        addRef();
        return static_cast<FtpStorageFactory*>(this);
    }
    else if (std::memcmp(&interfaceID,
                            &nxpl::IID_PluginInterface,
                            sizeof(nxpl::IID_PluginInterface)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

int FtpStorageFactory::addRef() const
{
    return p_addRef();
}

int FtpStorageFactory::releaseRef() const
{
    return p_releaseRef();
}

const char** FtpStorageFactory::findAvailable() const
{
    assert(false);
    return nullptr;
}

Storage* FtpStorageFactory::createStorage(const char* url, int* ecode)
{
    Storage* ret = nullptr;
    *ecode = error::NoError;
    try
    {
        ret = new FtpStorage(url);
    }
    catch (...)
    {
        if (ecode)
            *ecode = error::UrlNotExists;
        return nullptr;
    }
    return ret;
}

const char* STORAGE_METHOD_CALL FtpStorageFactory::storageType() const
{
    return "ftp";
}

#define ERROR_LIST(APPLY) \
APPLY(nx_spl::error::EndOfFile) \
APPLY(nx_spl::error::NoError) \
APPLY(nx_spl::error::NotEnoughSpace) \
APPLY(nx_spl::error::ReadNotSupported) \
APPLY(nx_spl::error::SpaceInfoNotAvailable) \
APPLY(nx_spl::error::StorageUnavailable) \
APPLY(nx_spl::error::UnknownError) \
APPLY(nx_spl::error::UrlNotExists) \
APPLY(nx_spl::error::WriteNotSupported)

#define STR_ERROR(ecode) case ecode: return #ecode;

const char* FtpStorageFactory::lastErrorMessage(int ecode) const
{
    switch(ecode)
    {
        ERROR_LIST(STR_ERROR);
    }
    return "";
}
#undef PRINT_ERROR
#undef ERROR_LIST


// Ftp IO Device
FtpIODevice::FtpIODevice(FtpImplPtr ftpImplPtr, const char *uri, int mode):
    m_impl(ftpImplPtr),
    m_mode(mode),
    m_pos(0),
    m_uri(aux::toRelativeFtpPath(uri)),
    m_altered(false),
    m_localsize(0)
{
    try
    {
        std::string remoteDir, remoteFile;
        aux::dirFromUri(m_uri, &remoteDir, &remoteFile);
        m_localfile = aux::localUniqueFilePath("--" + remoteFile);
        const bool fileExists = m_impl->pathExists(m_uri);
        if (mode & io::WriteOnly)
        {
            if (!fileExists)
            {
                FILE *f = fopen(m_localfile.fullPath.c_str(), "wb");
                if (f == NULL)
                    throw std::runtime_error("couldn't create local temporary file");
                fclose(f);
                m_impl->makePath(remoteDir);
                if (!m_impl->put(m_localfile.fullPath, m_uri))
                    throw std::runtime_error("ftp put failed");
            }
            else
            {
                if (!m_impl->get(m_localfile.fullPath, m_uri))
                    throw std::runtime_error("ftp get failed");
            }
        }
        else if (mode & io::ReadOnly)
        {
            if (!fileExists)
                throw std::runtime_error("file not found in storage");
            else if (!m_impl->get(m_localfile.fullPath, m_uri))
                throw std::runtime_error("ftp get failed");
        }

        // calculate local file size
        if ((m_localsize = aux::getFileSize(m_localfile.fullPath.c_str())) == -1)
            throw std::runtime_error("local file calculate size failed");
    }
    catch(...)
    {
        throw;
    }
}

FtpIODevice::~FtpIODevice()
{
    flush();
    remove(m_localfile.fullPath.c_str());
}

void FtpIODevice::flush()
{
    if(m_altered)
        m_impl->put(m_localfile.fullPath, m_uri);
}

uint32_t FtpIODevice::write(const void* src, const uint32_t size, int* ecode)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (ecode)
        *ecode = error::NoError;

    if (!(m_mode & io::WriteOnly))
    {
        *ecode = error::WriteNotSupported;
        return 0;
    }

    FILE * f = fopen(m_localfile.fullPath.c_str(), "r+b");
    if (f == NULL)
        goto bad_end;

    if (fseek(f, (int)m_pos, SEEK_SET) != 0)
        goto bad_end;

    fwrite(src, 1, size, f);
    m_pos += size;
    m_localsize += size;
    m_altered = true;
    fclose(f);
    return size;

bad_end:
    if (f != NULL)
        fclose(f);
    *ecode = error::UnknownError;
    return 0;
}

uint32_t STORAGE_METHOD_CALL FtpIODevice::read(
    void*           dst,
    const uint32_t  size,
    int*            ecode
) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t readSize = 0;
    if (ecode)
        *ecode = error::NoError;

    if (!(m_mode & io::ReadOnly))
    {
        *ecode = error::ReadNotSupported;
        return 0;
    }

    FILE * f = fopen(m_localfile.fullPath.c_str(), "rb");
    if (f == NULL)
        goto bad_end;

    readSize = (uint32_t)(m_pos + size > m_localsize ? m_localsize - m_pos : size);

    if (fseek(f, (int)m_pos, SEEK_SET) != 0)
        goto bad_end;

    fread(dst, 1, readSize, f);
    m_pos += readSize;
    fclose(f);
    return readSize;

bad_end:
    if (f != NULL)
        fclose(f);
    *ecode = error::UnknownError;
    return 0;
}

int STORAGE_METHOD_CALL FtpIODevice::seek(
    uint64_t    pos,
    int*        ecode
)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (ecode)
        *ecode = error::NoError;

    if ((long long)pos > m_localsize)
    {
        *ecode = error::UnknownError;
        return 0;
    }

    m_pos = pos;
    return 1;
}

int STORAGE_METHOD_CALL FtpIODevice::getMode() const
{
    return m_mode;
}

uint32_t STORAGE_METHOD_CALL FtpIODevice::size(int* ecode) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (ecode)
        *ecode = error::NoError;

    long long ret;
    if ((ret = aux::getFileSize(m_localfile.fullPath.c_str())) == -1)
    {
        if (ecode)
            *ecode = error::UnknownError;
        return 0;
    }
    return static_cast<uint32_t>(ret);
}

void* FtpIODevice::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (std::memcmp(&interfaceID,
                    &IID_IODevice,
                    sizeof(nxpl::NX_GUID)) == 0)
    {
        addRef();
        return static_cast<nx_spl::IODevice*>(this);
    }
    else if (std::memcmp(&interfaceID,
                            &nxpl::IID_PluginInterface,
                            sizeof(nxpl::IID_PluginInterface)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

int FtpIODevice::addRef() const
{
    return p_addRef();
}

int FtpIODevice::releaseRef() const
{
    return p_releaseRef();
}

} // namespace nx_spl

extern "C"
{
    NX_PLUGIN_API nxpl::PluginInterface* createNXPluginInstance()
    {
        return new nx_spl::FtpStorageFactory();
    }
}
