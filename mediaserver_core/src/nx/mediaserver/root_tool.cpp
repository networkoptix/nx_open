#include <QFileInfo>
#include <QDir>
#include <utils/fs/file.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/system_error.h>
#include <nx/utils/concurrent.h>
#if defined (Q_OS_LINUX)
    #include <nx/system_commands/domain_socket/read_linux.h>
#endif
#include "root_tool.h"

#if defined(Q_OS_WIN)
    #include <BaseTsd.h>
    #define ssize_t SSIZE_T
#endif

namespace nx {
namespace mediaserver {

RootTool::RootTool(const QString& toolPath):
    m_toolPath(toolPath)
{
    NX_INFO(this, lm("Initialized: %1").arg(toolPath));
}

Qn::StorageInitResult RootTool::mount(const QUrl& url, const QString& path)
{
#if defined(Q_OS_LINUX)
    makeDirectory(path);
    auto mountResultToStorageInitResult =
        [&](SystemCommands::MountCode mountResult)
        {
            switch (mountResult)
            {
                case SystemCommands::MountCode::ok:
                    return Qn::StorageInitResult::StorageInit_Ok;
                    break;
                case SystemCommands::MountCode::otherError:
                    return Qn::StorageInitResult::StorageInit_WrongPath;
                    break;
                case SystemCommands::MountCode::wrongCredentials:
                    return Qn::StorageInitResult::StorageInit_WrongAuth;
                    break;
            }
            return Qn::StorageInitResult::StorageInit_WrongPath;
        };

    auto logMountResult =
        [&](SystemCommands::MountCode mountResult, bool viaSocket)
        {
            QString viaString = viaSocket ? "via call to the root tool" : "via direct command";
            switch (mountResult)
            {
            case SystemCommands::MountCode::ok:
                NX_VERBOSE(
                    this, lm("[mount] Successfully mounted %1 to %2 %3").args(url, path, viaString));
                break;
            case SystemCommands::MountCode::otherError:
                NX_WARNING(
                    this, lm("[mount] Failed to mount %1 to %2 %3").args(url, path, viaString));
                break;
            case SystemCommands::MountCode::wrongCredentials:
                NX_WARNING(
                    this,
                    lm("[mount] Failed to mount %1 to %2 %3 due to WRONG credentials")
                        .args(url, path, viaString));
                break;
            }
        };

    using namespace boost;

    auto uncString = "//" + url.host() + url.path();
    if (m_toolPath.isEmpty())
    {
        auto userName = url.userName().toStdString();
        auto password = url.password().toStdString();
        auto mountResult = SystemCommands().mount(
            uncString.toStdString(), path.toStdString(),
            userName.empty() ? none : boost::optional<std::string>(userName),
            password.empty() ? none : boost::optional<std::string>(password),
            false);

        logMountResult(mountResult, false);
        return mountResultToStorageInitResult(mountResult);
    }

    std::vector<QString> args = {"mount", uncString, path};

    if (!url.userName().isEmpty())
        args.push_back(url.userName());

    if (!url.password().isEmpty())
        args.push_back(url.password());


    QnMutexLocker lock(&m_mutex);
    utils::concurrent::run([&]() { execute(args); });

    int64_t result = (int64_t) SystemCommands::MountCode::otherError;
    system_commands::domain_socket::readInt64(&result);
    logMountResult((SystemCommands::MountCode) result, true);

    return mountResultToStorageInitResult((SystemCommands::MountCode) result);
#else
    return Qn::StorageInitResult::StorageInit_WrongPath;
#endif
}

Qn::StorageInitResult RootTool::remount(const QUrl& url, const QString& path)
{
    SystemCommands::UnmountCode result = unmount(path);
    NX_VERBOSE(
        this,
        lm("[initOrUpdate, mount] root_tool unmount %1 result %2")
            .args(path, SystemCommands::unmountCodeToString(result)));

    auto mountResult = mount(url, path);
    NX_VERBOSE(
        this,
        lm("[initOrUpdate, mount] root_tool mount %1 to %2 result %3").args(url, path, mountResult));

    return mountResult;
}

SystemCommands::UnmountCode RootTool::unmount(const QString& path)
{
#if defined (Q_OS_LINUX)
    return commandHelper(
        SystemCommands::UnmountCode::noPermissions, path, "unmount",
        [path]() { return SystemCommands().unmount(path.toStdString(), /*reportViaSocket*/ false); },
        []()
        {
            int64_t result;
            return system_commands::domain_socket::readInt64(&result)
                ? (SystemCommands::UnmountCode) result : SystemCommands::UnmountCode::noPermissions;
        });
#else
    return SystemCommands::UnmountCode::noPermissions;
#endif
}

bool RootTool::changeOwner(const QString& path)
{
    if (m_toolPath.isEmpty())
    {
        SystemCommands commands;
        if (!commands.changeOwner(path.toStdString()))
            return false;

        return true;
    }

    return execute({"chown", path}) == 0;
}

bool RootTool::touchFile(const QString& path)
{
    if (m_toolPath.isEmpty())
    {
        SystemCommands commands;
        if (!commands.touchFile(path.toStdString()))
            return false;

        return true;
    }

    return execute({"touch", path}) == 0;
}

bool RootTool::makeDirectory(const QString& path)
{
    if (m_toolPath.isEmpty())
    {
        SystemCommands commands;
        if (!commands.makeDirectory(path.toStdString()))
            return false;

        return true;
    }

    return execute({"mkdir", path}) == 0;
}

bool RootTool::removePath(const QString& path)
{
    if (m_toolPath.isEmpty())
    {
        SystemCommands commands;
        if (!commands.removePath(path.toStdString()))
            return false;

        return true;
    }

    return execute({"rm", path}) == 0;
}

bool RootTool::rename(const QString& oldPath, const QString& newPath)
{
    if (m_toolPath.isEmpty())
    {
        SystemCommands commands;
        if (!commands.rename(oldPath.toStdString(), newPath.toStdString()))
            return false;

        return true;
    }

    return execute({"mv", oldPath, newPath}) == 0;
}

template<typename R, typename DefaultAction, typename SocketAction, typename... Args>
R RootTool::commandHelper(
    R defaultValue, const QString& path, const char* command,
    DefaultAction defaultAction, SocketAction socketAction, Args&&... args)
{
    if (m_toolPath.isEmpty())
        return defaultAction();

    QnMutexLocker lock(&m_mutex);
    utils::concurrent::run([this, path, command, args...]() { execute({command, path, args...}); });

    return socketAction();
}

template<typename DefaultAction>
qint64 RootTool::int64SingleArgCommandHelper(
    const QString& path, const char* command, DefaultAction defaultAction)
{
#if defined (Q_OS_LINUX)
    return commandHelper(
        -1LL, path, command, defaultAction,
        []()
        {
            int64_t result;
            return system_commands::domain_socket::readInt64(&result) ? result : -1;
        });
#else
    return -1;
#endif
}

int RootTool::open(const QString& path, QIODevice::OpenMode mode)
{
#if defined (Q_OS_LINUX)
    int sysFlags = makeUnixOpenFlags(mode);
    if (m_toolPath.isEmpty())
        return SystemCommands().open(path.toStdString(), sysFlags, /*reportViaSocket*/ false);

    QnMutexLocker lock(&m_mutex);
    utils::concurrent::run(
        [this, path, sysFlags]() { execute({"open", path, QString::number(sysFlags)}); });
    auto result = system_commands::domain_socket::readFd();
    return result;
#else
    return -1;
#endif
}

qint64 RootTool::freeSpace(const QString& path)
{
    return int64SingleArgCommandHelper(
        path, "freeSpace",
        [path]() { return SystemCommands().freeSpace(path.toStdString(), /*reportViaSocket*/ false); });
}

qint64 RootTool::totalSpace(const QString& path)
{
    return int64SingleArgCommandHelper(
        path, "totalSpace",
        [path]() { return SystemCommands().totalSpace(path.toStdString(), /*reportViaSocket*/ false); });
}

bool RootTool::isPathExists(const QString& path)
{
#if defined (Q_OS_LINUX)
    return commandHelper(
        false, path, "exists",
        [path]() { return SystemCommands().isPathExists(path.toStdString(), /*reportViaSocket*/ false); },
        []()
        {
            int64_t result;
            return system_commands::domain_socket::readInt64(&result) ? (bool) result : false;
        });
#else
    return false;
#endif
}

static void* readBufferReallocCallback(void* context, ssize_t size)
{
    std::string* buf = reinterpret_cast<std::string*>(context);
    buf->resize(size);
    return (void*) buf->data();
}

struct StringRef
{
    const char* data;
    ssize_t size;
};

static bool extractSubstring(const char** pdata, StringRef* stringRef)
{
    const char* current = *pdata;
    while (*current && *current != ',')
        ++current;

    if (current == *pdata)
        return false;

    stringRef->data = *pdata;
    stringRef->size = current - *pdata;

    if (*current)
        *pdata = current + 1;

    return true;
}

static QnAbstractStorageResource::FileInfoList fileListFromSerialized(const char* data)
{
    QnAbstractStorageResource::FileInfoList result;
    StringRef stringRef;

    while (true)
    {
        if (!extractSubstring(&data, &stringRef))
            break;

        std::string name(stringRef.data, stringRef.size);

        if (!extractSubstring(&data, &stringRef))
            break;

        int64_t size = strtoll(stringRef.data, nullptr, 10);

        if (!extractSubstring(&data, &stringRef))
            break;

        bool isDir = strtol(stringRef.data, nullptr, 10);

        result.append(QnAbstractStorageResource::FileInfo(QString::fromStdString(name), size, isDir));
    }

    return result;
}

template<typename DefaultAction>
std::string RootTool::stringCommandHelper(
    const QString& path, const char* command, DefaultAction action)
{
#if defined (Q_OS_LINUX)
    if (m_toolPath.isEmpty())
        return action();

    QnMutexLocker lock(&m_mutex);
    utils::concurrent::run([this, path, command]() { execute({command, path}); });

    std::string buf;
    if (!system_commands::domain_socket::readBuffer(&readBufferReallocCallback, &buf))
        return "";

    return buf;
#else
    return "";
#endif
}

QnAbstractStorageResource::FileInfoList RootTool::fileList(const QString& path)
{
    return fileListFromSerialized(
        stringCommandHelper(
            path, "list",
            [path]()
            {
                return SystemCommands().serializedFileList(path.toStdString(), false).c_str();
            }).c_str());
}

qint64 RootTool::fileSize(const QString& path)
{
    return int64SingleArgCommandHelper(
        path, "size",
        [path]() { return SystemCommands().fileSize(path.toStdString(), /*reportViaSocket*/ false); });
}

QString RootTool::devicePath(const QString& fsPath)
{
    auto result = QString::fromStdString(
        stringCommandHelper(
            fsPath, "devicePath",
            [fsPath]()
            {
                return SystemCommands().devicePath(fsPath.toStdString(), false).c_str();
            }));

    return result;
}

static std::string makeArgsLine(const std::vector<QString>& args)
{
    std::ostringstream argsStream;
    for (const auto& arg: args)
        argsStream << "'" << arg.toStdString() << "' ";

    return argsStream.str();
}

static std::string makeCommand(const QString& toolPath, const std::string& argsLine)
{
    std::ostringstream runStream;
    runStream << "'" << toolPath.toStdString() << "' " << argsLine << "2>&1";
    return runStream.str();
}

int  RootTool::execute(const std::vector<QString>& args)
{
#if defined( Q_OS_LINUX )
    const auto argsLine = makeArgsLine(args);
    const auto commandLine = makeCommand(m_toolPath, argsLine);
    const auto pipe = popen(commandLine.c_str(), "r");
    if (pipe == nullptr)
    {
        NX_DEBUG(this, lm("Popen %1 has failed: %2").args(
            m_toolPath, SystemError::getLastOSErrorText()));

        return false;
    }

    std::ostringstream outputStream;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL)
        outputStream << buffer;

    const auto resultCode = pclose(pipe);
    NX_DEBUG(this, lm("%1 -- %2").args(argsLine, resultCode));

    auto output = outputStream.str();
    output.erase(output.find_last_not_of(" \n\r\t") + 1);
    if (output.size() != 0)
        NX_VERBOSE(this, output);

    return resultCode;
#else
    NX_ASSERT(false, "Only linux is supported so far");
    return false;
#endif
}

std::unique_ptr<RootTool> findRootTool(const QString& applicationPath)
{
    const auto toolPath = QFileInfo(applicationPath).dir().filePath("root_tool");
    bool isRootToolExists = QFileInfo(toolPath).exists();
    if (!isRootToolExists)
        NX_WARNING(typeid(RootTool), lm("Executable does not exist: %1").arg(toolPath));

    return std::make_unique<RootTool>(isRootToolExists ? toolPath : QString());
}

} // namespace mediaserver
} // namespace nx

