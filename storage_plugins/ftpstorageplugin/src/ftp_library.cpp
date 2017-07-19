#include <cstring>
#include <atomic>
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <cassert>
#include <ctime>
#include <cstdlib>

#ifdef __linux__
#   include <sys/stat.h>
#endif

#include "ftp_library.h"

#ifdef _MSC_VER
#   define NOEXCEPT
#elif defined __GNUC__
#   define NOEXCEPT noexcept
#endif

namespace nx_spl
{

// auxiliary data structures/functions
namespace aux
{   // Some custom exceptions.
// Generally we want to distinguish nature of bad things happened.
class NetworkException
    : public std::runtime_error
{
public:
    explicit NetworkException(const char* s)
        : runtime_error(s)
    {}

    virtual const char* what() const NOEXCEPT
    {
        return runtime_error::what();
    }
};

class BadUrlException
    : public std::runtime_error
{
public:
    explicit BadUrlException(const char* s)
        : runtime_error(s)
    {}

    virtual const char* what() const NOEXCEPT
    {
        return runtime_error::what();
    }
};

class ConnectException
    : public std::runtime_error
{
public:
    explicit ConnectException(const char* s)
        : runtime_error(s)
    {}

    virtual const char* what() const NOEXCEPT
    {
        return runtime_error::what();
    }
};


class InternalErrorException
    : public std::runtime_error
{
public:
    explicit InternalErrorException(const char* s)
        : runtime_error(s)
    {}

    virtual const char* what() const NOEXCEPT
    {
        return runtime_error::what();
    }
};

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
    FILE            *m_f;
    int             *m_ecode;
    error::code_t    m_err;
};

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
                if (cur == s.size()) // That's all? No, url can't end here
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
                if (cur == s.size()) // That's all? No, url can't end here
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
                if (cur == s.size())
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

                if (cur == s.size())
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

// Generates pseudo-random string for use as unique file name
// Strictly speaking, the uniquness is not guaranteed, so delete files as soon as possible.
std::string getRandomFileName()
{
    std::stringstream randomStringStream;
    randomStringStream << std::hex << std::rand() << std::rand();

    return randomStringStream.str();
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
        if (pos < uri.size() - 1) // if file name is not empty
            file->assign(uri.begin() + pos + 1, uri.end());
    }
}

// Checks if remote file exists. Bases on MLSD command response parsing.
bool remoteFileExists(const std::string& uri, ftplib& impl)
{
    std::string tmpFile(getRandomFileName());
    FileRemover fr(tmpFile);
    std::string dir;
    std::string file;
    dirFromUri(uri, &dir, &file);

    if (file.empty()) // Uri shouldn't end with '/'
        throw std::runtime_error("empty file name");

    if (impl.Nlst(tmpFile.c_str(), dir.c_str()) == 0)
        throw aux::BadUrlException("MLSD failed. Remote dir not exists");

    std::ifstream ifs(tmpFile);
    if (!ifs)
        throw std::runtime_error("Local file open failed");

    std::string line;
    bool found = false;
    while (!ifs.eof())
    {
        std::getline(ifs, line);
        std::string::size_type pos = line.find(file);
        // check that match is exact. ('a.txt' query shouldn't trigger for 'aaa.txt')
        if (pos != std::string::npos && (pos == 0 || line[pos-1] == ';' || line[pos-1] == ' '))
        {
            found = true;
            break;
        }
    }
    ifs.close();
    return found;
}

// set error code to initial state (NoError generally if storage is available)
error::code_t checkECode(
    int            *checked,                // ecode to set
    const int       avail,                  // pass result of getAvail here
    error::code_t   toSet = error::NoError  // default ecode
)
{
    if (checked)
        *checked = error::NoError;

    if (!avail)
    {
        if (checked)
            *checked = error::StorageUnavailable;
        return error::StorageUnavailable;
    }
    else if (checked)
        *checked = toSet;

    return (error::code_t)*checked;
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

#elif defined __linux__

    struct stat st;
    if (stat(fname, &st) == -1)
        return -1;
    return st.st_size;
#endif
}

void establishFtpConnection(
    const std::string              *rawUrl,     // In (May be NULL)
    std::string                    &url,        // InOut (Out if rawUrl is not null, otherwise should be correctly set)
    std::string                    &uname,      // InOut (Out if rawUrl is not null, otherwise should be correctly set)
    std::string                    &upasswd,    // InOut (Out if rawUrl is not null, otherwise should be correctly set)
    implPtrType                    &impl        // Out
)
{
    try
    {
        impl.reset(new ftplib);
    }
    catch (const std::exception& e)
    {
        throw aux::NetworkException(e.what());
    }

    if (rawUrl)
    {
        aux::Url u;
        try
        {
            u = aux::Url::fromString(*rawUrl);
        }
        catch (const std::exception& e)
        {
            throw aux::BadUrlException(e.what());
        }
        url = u.host + ':' + (u.port.empty() ? std::string("21") : u.port);
        uname = u.uname.empty() ? "anonymous" : u.uname.c_str();
        upasswd = u.upasswd.empty() ? "" : u.upasswd.c_str();
    }

    try
    {
        if (impl->Connect(url.c_str()) == 0)
            throw aux::ConnectException("Couldn't connect");

        if (impl->Login(uname.c_str(), upasswd.c_str()) == 0)
        {
            impl->Quit();
            throw aux::ConnectException("couldn't login");
        }
    }
    catch (const std::exception&)
    {
        if (impl)
            impl->Quit();
        throw;
    }
}

int tryFtpReconnect(
    const implPtrType   &impl,
    const std::string   &url,
    const std::string   &uname,
    const std::string   &upasswd,
    int                 *ecode = nullptr
)
{
    impl->Quit();
    if (impl->Connect(url.c_str()) == 0)
        return 0;

    if (impl->Login(uname.c_str(), upasswd.c_str()) == 0)
    {
        impl->Quit();
        return 0;
    }

    if(ecode)
        *ecode = error::NoError;
    return 1;
}

} // namespace aux

// FileInfo Iterator
FtpFileInfoIterator::FtpFileInfoIterator(
    ftplib             &impl,
    FileListType      &&fileList,
    const std::string  &baseDir
)
    : m_impl(impl),
        m_fileList(std::move(fileList)),
        m_curFile(m_fileList.cbegin())
{
    m_urlData.assign(baseDir.cbegin(), baseDir.cend());
    if (baseDir[baseDir.size()-1] != '/')
    {
        m_urlData.push_back('/');
        m_basedirsize = (int)(baseDir.size() + 1);
    }
    else
        m_basedirsize = (int)baseDir.size();
}

FtpFileInfoIterator::~FtpFileInfoIterator()
{}

int FtpFileInfoIterator::fileInfoFromMLSDString(
    const char  *mlsd,  // In. Null-terminated.
    FileInfo    *fi,    // Out.
    char        *urlBuf // Out. Should be vaild memory.
) const
{
    // MLSD response is array of 'attr1=val1;attr2=val2;...; fileName' strings
    char buf[1024];
    strcpy(buf, mlsd);
    const char* vp = strtok(buf, ";");
    memset(fi, '\0', sizeof(FileInfo));
    while (vp)
    {
        const char* eq;
        if (eq = strchr(vp, '='))
        {
            if (strncmp(vp, "size", 4) == 0 || strncmp(vp, "SIZE", 4) == 0)
                fi->size = strtoul(eq+1, NULL, 10);
            else if (strncmp(vp, "type", 4) == 0 || strncmp(vp, "TYPE", 4) == 0)
            {
                ++eq;
                if ((eq[0] == 'd' || eq[0] == 'D') &&
                    (eq[1] == 'i' || eq[1] == 'I') &&
                    (eq[2] == 'r' || eq[0] == 'R'))
                {
                    fi->type = isDir;
                }
                else if ((eq[0] == 'f' || eq[0] == 'F') &&
                            (eq[1] == 'i' || eq[1] == 'I') &&
                            (eq[2] == 'l' || eq[0] == 'L'))
                {
                    fi->type = isFile;
                }
            }
        }
        else
        {
            while (isspace(*vp))
                ++vp;
            memcpy(urlBuf, vp, strlen(vp));
            urlBuf[strlen(vp)] = '\0';
        }
        vp = strtok(NULL, ";");
    }
    return 0;
}

FileInfo* STORAGE_METHOD_CALL FtpFileInfoIterator::next(int* ecode) const
{
    if (ecode)
        *ecode = nx_spl::error::NoError;

    if (m_curFile != m_fileList.cend())
    {
        m_urlData.resize(m_curFile->size() + m_urlData.size());
        fileInfoFromMLSDString(
            m_curFile->c_str(),
            &m_fileInfo,
            m_urlData.data() + m_basedirsize
        );
        m_fileInfo.url = m_urlData.data();
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

unsigned int FtpFileInfoIterator::addRef()
{
    return p_addRef();
}

unsigned int FtpFileInfoIterator::releaseRef()
{
    return p_releaseRef();
}


// Ftp Storage
FtpStorage::FtpStorage(const std::string& url)
    : m_available(false)
{
    try
    {
        aux::establishFtpConnection(
            &url,
            m_implurl,
            m_user,
            m_passwd,
            m_impl
        );
    }
    catch(const aux::ConnectException&)
    {
        m_impl->Quit();
        m_available = false;
        return;
    }
    catch(...)
    {
        m_impl->Quit();
        throw;
    }
    m_available = true;
}

FtpStorage::~FtpStorage()
{
    m_impl->Quit();
}

int STORAGE_METHOD_CALL FtpStorage::isAvailable() const
{   // If PASV failed it means that either there are some network problems
    // or just we've been idle for too long and server has closed control session.
    // In the latter case we can try to reestablish connection.

    std::lock_guard<std::mutex> lock(m_mutex);
    m_available = m_impl->TestControlConnection();
    if (!m_available)
        m_available = aux::tryFtpReconnect(
            m_impl,
            m_implurl,
            m_user,
            m_passwd
        );
    return m_available;
}

uint64_t STORAGE_METHOD_CALL FtpStorage::getFreeSpace(int* ecode) const
{   // In general there is no reliable way to determine FTP server free disk space
    //if (ecode)
    //    *ecode = error::SpaceInfoNotAvailable;
    //return unknown_size;
    if (ecode)
        *ecode = error::NoError;

    return 1000LL * 1000 * 1000 * 100; // for tests
}

uint64_t STORAGE_METHOD_CALL FtpStorage::getTotalSpace(int* ecode) const
{   // In general there is no reliable way to determine FTP server total disk space
    //if (ecode)
    //    *ecode = error::SpaceInfoNotAvailable;
    //return unknown_size;
    if (ecode)
        *ecode = error::NoError;

    return 1000LL * 1000 * 1000 * 100; // for tests
}

int STORAGE_METHOD_CALL FtpStorage::getCapabilities() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!getAvail())
        return 0;

    std::string fileName(aux::getRandomFileName());
    aux::FileRemover fr(fileName);

    // list
    int ret = 0;
    if (m_impl->Dir(fileName.c_str(), ".") == 1)
        ret |= cap::ListFile;

    FILE* ofs = fopen(fileName.c_str(), "wb");
    // write file
    if (ofs)
    {
        fwrite("1", 1, 2, ofs);
        fclose(ofs);
        if (m_impl->Put(fileName.c_str(),
                        fileName.c_str(),
                        ftplib::image) != 0)
        {
            ret |= cap::WriteFile;
        }
    }
    else
        return ret;

    // read file
    if (m_impl->Get(fileName.c_str(),
                    fileName.c_str(),
                    ftplib::image) != 0)
    {
        ret |= cap::ReadFile;
    }

    // remove file
    if (m_impl->Delete(fileName.c_str()) != 0)
        ret |= cap::RemoveFile;

    return ret;
}

void STORAGE_METHOD_CALL FtpStorage::removeFile(
    const char  *url,
    int         *ecode
)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(aux::checkECode(ecode, getAvail()) != nx_spl::error::NoError)
        return;
    if (m_impl->Delete(url) == 0 && ecode)
        *ecode = error::UnknownError;
}

void STORAGE_METHOD_CALL FtpStorage::removeDir(
    const char  *url,
    int         *ecode
)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(aux::checkECode(ecode, getAvail()) != nx_spl::error::NoError)
        return;

    if (m_impl->Rmdir(url) == 0 && ecode)
        *ecode = error::UnknownError;
}

void STORAGE_METHOD_CALL FtpStorage::renameFile(
    const char*     oldUrl,
    const char*     newUrl,
    int*            ecode
)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(aux::checkECode(ecode, getAvail()) != nx_spl::error::NoError)
        return;

    if (m_impl->Rename(oldUrl, newUrl) == 0 && ecode)
        *ecode = error::UnknownError;
}

FileInfoIterator* STORAGE_METHOD_CALL FtpStorage::getFileIterator(
    const char*     dirUrl,
    int*            ecode
) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(aux::checkECode(ecode, getAvail()) != nx_spl::error::NoError)
        return nullptr;

    std::string fileName(aux::getRandomFileName());
    aux::FileRemover fr(fileName);

    if (m_impl->Nlst(fileName.c_str(), dirUrl) == 0)
    {
        *ecode = error::UnknownError;
        return nullptr;
    }

    std::ifstream ifs(fileName);
    if (!ifs)
    {
        *ecode = error::UnknownError;
        return nullptr;
    }

    std::string line;
    std::vector<std::string> urls;

    while (!ifs.eof())
    {
        std::getline(ifs, line);
        if (!line.empty())
            urls.push_back(line);
    }

    return new FtpFileInfoIterator(
        *m_impl,
        std::move(urls),
        dirUrl
    );
}

int STORAGE_METHOD_CALL FtpStorage::fileExists(
    const char  *url,
    int         *ecode
) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(aux::checkECode(ecode, getAvail()) != nx_spl::error::NoError)
        return 0;

    try
    {
        if (!aux::remoteFileExists(url, *m_impl))
            return 0;
        else
            return 1;
    }
    catch (const std::exception&)
    {
        *ecode = error::UnknownError;
        return 0;
    }
}

int STORAGE_METHOD_CALL FtpStorage::dirExists(
    const char  *url,
    int         *ecode
) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(aux::checkECode(ecode, getAvail()) != nx_spl::error::NoError)
        return 0;

    std::string fileName(aux::getRandomFileName());
    aux::FileRemover fr(fileName);

    if (m_impl->Nlst(fileName.c_str(), url) == 0)
        return 0;

    return 1;
}

uint64_t STORAGE_METHOD_CALL FtpStorage::fileSize(
    const char*     url,
    int*            ecode
) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(aux::checkECode(ecode, getAvail()) != nx_spl::error::NoError)
        return 0;

    int size;

    if (m_impl->Size(url, &size, ftplib::image) == 0)
    {
        *ecode = error::UnknownError;
        return unknown_size;
    }

    return size;
}

IODevice* STORAGE_METHOD_CALL FtpStorage::open(
    const char*     uri,
    int             flags,
    int*            ecode
) const
{
    *ecode = error::NoError;
    IODevice *ret = nullptr;
    if (!getAvail())
    {
        *ecode = error::StorageUnavailable;
        return nullptr;
    }

    try
    {
        ret = new FtpIODevice(
            uri,
            flags,
            m_implurl,
            m_user,
            m_passwd
        );
    }
    catch (const aux::BadUrlException&)
    {
        *ecode = error::UrlNotExists;
        return nullptr;
    }
    catch (const aux::InternalErrorException&)
    {
        *ecode = error::UnknownError;
        return nullptr;
    }
    catch (const std::exception&)
    {
        *ecode = error::UnknownError;
        return nullptr;
    }
    return ret;
}

void* FtpStorage::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (std::memcmp(&interfaceID,
                    &IID_Storage,
                    sizeof(nxpl::NX_GUID)) == 0)
    {
        addRef();
        return static_cast<nx_spl::Storage*>(this);
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

unsigned int FtpStorage::addRef()
{
    return p_addRef();
}

unsigned int FtpStorage::releaseRef()
{
    return p_releaseRef();
}


// FtpStorageFactory
FtpStorageFactory::FtpStorageFactory()
{
    std::srand(time(0));
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

unsigned int FtpStorageFactory::addRef()
{
    return p_addRef();
}

unsigned int FtpStorageFactory::releaseRef()
{
    return p_releaseRef();
}

const char** STORAGE_METHOD_CALL FtpStorageFactory::findAvailable() const
{
    assert(false);
    return nullptr;
}

Storage* STORAGE_METHOD_CALL FtpStorageFactory::createStorage(
    const char* url,
    int*        ecode
)
{
    Storage* ret = nullptr;
    *ecode = error::NoError;
    try
    {
        ret = new FtpStorage(url);
    }
    catch (const std::bad_alloc&)
    {
        if (ecode)
            *ecode = error::UnknownError;
        return nullptr;
    }
    catch (const aux::NetworkException&)
    {
        if (ecode)
            *ecode = error::UnknownError;
        return nullptr;
    }
    catch (const aux::BadUrlException&)
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
FtpIODevice::FtpIODevice(
    const char         *uri,
    int                 mode,
    const std::string  &storageUrl,
    const std::string  &uname,
    const std::string  &upasswd
)
    : m_mode(mode),
        m_pos(0),
        m_uri(uri),
        m_altered(false),
        m_localsize(0),
        m_implurl(storageUrl),
        m_user(uname),
        m_passwd(upasswd)
{
    //  If file opened for read-only and no such file uri in storage throw BadUrl
    //  If file opened for write and no such file uri in stor - create it.
    std::lock_guard<std::mutex> lock(m_mutex);
    try
    {
        aux::establishFtpConnection(
            nullptr,
            m_implurl,
            m_user,
            m_passwd,
            m_impl
        );
        std::string remoteDir, remoteFile;
        aux::dirFromUri(uri, &remoteDir, &remoteFile);
        m_localfile = aux::getRandomFileName() + "_" + remoteFile;
        bool fileExists = false;
        try
        {
            fileExists = aux::remoteFileExists(uri, *m_impl);
        }
        catch (const aux::BadUrlException& e)
        {
            if (m_impl->Mkdir(remoteDir.c_str()) == 0)
                throw aux::InternalErrorException(e.what());
            else fileExists = false;
        }
        catch (const std::exception& e)
        {
            throw aux::InternalErrorException(e.what());
        }

        if (mode & io::WriteOnly)
        {
            if (!fileExists)
            {
                FILE *f = fopen(m_localfile.c_str(), "wb");
                if (f == NULL)
                    throw aux::InternalErrorException("couldn't create local temporary file");
                fclose(f);
                if (m_impl->Put(m_localfile.c_str(), uri, ftplib::image) == 0)
                    throw aux::InternalErrorException("ftp put failed");
            }
            else
            {
                if (m_impl->Get(m_localfile.c_str(), uri, ftplib::image) == 0)
                    throw aux::InternalErrorException("ftp get failed");
            }
        }
        else if (mode & io::ReadOnly)
        {
            if (!fileExists)
                throw aux::BadUrlException("file not found in storage");
            else if (m_impl->Get(m_localfile.c_str(), uri, ftplib::image) == 0)
                throw aux::InternalErrorException("ftp get failed");
        }

        // calculate local file size
        if ((m_localsize = aux::getFileSize(m_localfile.c_str())) == -1)
            throw aux::InternalErrorException("local file calculate size failed");
    }
    catch(...)
    {
        m_impl->Quit();
        throw;
    }
}

FtpIODevice::~FtpIODevice()
{
    flush();
    remove(m_localfile.c_str());
    m_impl->Quit();
}

// Since there is no explicit 'close' function,
// synchronization attempt is made in this function and
// this function is called from destructor.
// That's why all possible errors are discarded.
void FtpIODevice::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    int alive =
        m_impl->TestControlConnection() ?
            true :
            aux::tryFtpReconnect(
                m_impl,
                m_implurl,
                m_user,
                m_passwd,
                nullptr
            );

    if(alive && m_altered)
        m_impl->Put(m_localfile.c_str(), m_uri.c_str(), ftplib::image);
}

uint32_t STORAGE_METHOD_CALL FtpIODevice::write(
    const void     *src,
    const uint32_t  size,
    int            *ecode
)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (ecode)
        *ecode = error::NoError;

    if (!(m_mode & io::WriteOnly))
    {
        *ecode = error::WriteNotSupported;
        return 0;
    }

    FILE * f = fopen(m_localfile.c_str(), "r+b");
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

    FILE * f = fopen(m_localfile.c_str(), "rb");
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
    if ((ret = aux::getFileSize(m_localfile.c_str())) == -1)
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

unsigned int FtpIODevice::addRef()
{
    return p_addRef();
}

unsigned int FtpIODevice::releaseRef()
{
    return p_releaseRef();
}

} // namespace nx_spl

extern "C"
{
#ifdef _WIN32
    __declspec(dllexport)
#endif
    nxpl::PluginInterface* createNXPluginInstance()
    {
        return new nx_spl::FtpStorageFactory();
    }
}


