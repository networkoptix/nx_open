#include <QFileInfo>
#include <QDir>
#include <utils/fs/file.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/system_error.h>
#include <nx/utils/raii_guard.h>
#if defined (Q_OS_LINUX)
    #include <nx/system_commands/domain_socket/read_linux.h>
    #include <sys/wait.h>
    #include <sys/types.h>
    #include <sys/time.h>
    #include <unistd.h>
#endif
#include "root_tool.h"

#if defined(Q_OS_WIN)
    #include <BaseTsd.h>
    #define ssize_t SSIZE_T
#endif

namespace nx {
namespace mediaserver {

static std::string makeCommand(const QString& toolPath, const std::string& argsLine)
{
    std::ostringstream runStream;
    runStream << "'" << toolPath.toStdString() << "' " << argsLine << "2>&1";
    return runStream.str();
}

static std::string makeArgsLine(const std::vector<QString>& args)
{
    std::ostringstream argsStream;
    for (const auto& arg: args)
        argsStream << "'" << arg.toStdString() << "' ";

    return argsStream.str();
}

static const int kMaximumRootToolWaitTimeoutMs = 15 * 60 * 1000;

RootTool::RootTool(const QString& toolPath):
    m_toolPath(toolPath)
{
    NX_INFO(this, lm("Initialized: %1").arg(toolPath));
}

Qn::StorageInitResult RootTool::mount(const QUrl& url, const QString& path)
{
#if defined (Q_OS_LINUX)
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

    SystemCommands systemCommands;
    auto logMountResult =
        [&](SystemCommands::MountCode mountResult, bool viaSocket)
        {
            QString viaString = viaSocket ? "via call to the root tool" : "via direct command";
            switch (mountResult)
            {
            case SystemCommands::MountCode::ok:
                NX_DEBUG(this, lm(
                    "[mount] Successfully mounted '%1' to '%2' %3").args(url, path, viaString));
                break;
            case SystemCommands::MountCode::otherError:
                NX_WARNING(this, lm("[mount] Failed to mount '%1' to '%2' %3: %4").args(
                        url, path, viaString, systemCommands.lastError()));
                break;
            case SystemCommands::MountCode::wrongCredentials:
                NX_WARNING(this, lm(
                    "[mount] Failed to mount '%1' to '%2' %3 due to WRONG credentials").args(
                        url, path, viaString, systemCommands.lastError()));
                break;
            }
        };

    using namespace boost;

    auto uncString = "//" + url.host() + url.path();
    if (m_toolPath.isEmpty())
    {
        auto userName = url.userName().toStdString();
        auto password = url.password().toStdString();
        auto mountResult = systemCommands.mount(
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


    int64_t result = (int64_t) SystemCommands::MountCode::otherError;
    int socketPostfix = m_idHelper.take();
    execAndReadResult(
        socketPostfix,
        args,
        [&](){ return system_commands::domain_socket::readInt64(socketPostfix, &result); });
    logMountResult((SystemCommands::MountCode) result, true);

    return mountResultToStorageInitResult((SystemCommands::MountCode) result);

#else
    return Qn::StorageInitResult::StorageInit_WrongPath;
#endif
}

#if defined (Q_OS_LINUX)
template<typename Action>
void RootTool::execAndReadResult(int socketPostfix, const std::vector<QString>& args, Action action)
{
    auto onExitGuard = QnRaiiGuard::createDestructible(
        [this, socketPostfix]() { m_idHelper.putBack(socketPostfix); });

    std::vector<QString> augmentedArgs = { args[0], QString::number(socketPostfix) };
    std::copy(args.cbegin() + 1, args.cend(), std::back_inserter(augmentedArgs));

    int childPid = forkRoolTool(augmentedArgs);
    if (childPid < 0)
        return;

    action();
    waitForProc(childPid);
}
#endif

bool RootTool::execAndWait(const std::vector<QString>& args)
{
#if defined (Q_OS_LINUX)
    int childPid = forkRoolTool(args);
    if (childPid < 0)
        return false;

    return waitForProc(childPid);
#else
    return false;
#endif
}

#if defined (Q_OS_LINUX)
bool RootTool::waitForProc(int childPid)
{
    int result, status;
    struct timeval start, end;

    gettimeofday(&start, NULL); /*< #TODO: #akulikov Check error? */
    do
    {
        result = waitpid(childPid, &status, WNOHANG);
        if (result > 0)
        {
            if (WIFEXITED(status))
            {
                result = WEXITSTATUS(status);
                NX_VERBOSE(
                    this,
                    lm("Child process %1 finished %2")
                        .args(childPid, result == 0 ? "successfully" : "UNsuccessfully"));
                return result == 0;
            }
            NX_WARNING(
                this,
                lm("Child process %1 finished but getting exit status failed").args(childPid));

            return false;
        }
        else if (result < 0)
        {
            NX_ASSERT(false); /*< Child already exited without wait()? */
            return false;
        }

        usleep(1000);
        gettimeofday(&end, NULL);
    }
    while ((end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000 < kMaximumRootToolWaitTimeoutMs);

    NX_WARNING(this, lm("Child %1 seems to have hanged. Killing").args(childPid));

    int killProcId = forkRoolTool({"kill", QString::number(childPid)});
    waitForProc(killProcId);
    waitpid(childPid, NULL, 0);
    NX_VERBOSE(this, lm("Child %1 kill attempt commited").args(childPid));

    return false;
}
#endif

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
        [path, this]()
        {
            int64_t result = (int64_t) SystemCommands::UnmountCode::noPermissions;
            int socketPostfix = m_idHelper.take();
            execAndReadResult(
                socketPostfix,
                {"unmount", path},
                [&result, socketPostfix]()
                {
                    return system_commands::domain_socket::readInt64(socketPostfix, &result);
                });

            return (SystemCommands::UnmountCode) result;
        });
#else
    return SystemCommands::UnmountCode::noPermissions;
#endif
}

bool RootTool::changeOwner(const QString& path, bool recursive)
{
    if (m_toolPath.isEmpty())
    {
        SystemCommands commands;
        if (!commands.changeOwner(path.toStdString(), recursive))
            return false;

        return true;
    }

    return execAndWait({"chown", path, (recursive ? "recursive" : "")});
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

    return execAndWait({"mkdir", path});
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

    return execAndWait({"rm", path});
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

    return execAndWait({"mv", oldPath, newPath});
}

template<typename R, typename DefaultAction, typename SocketAction>
R RootTool::commandHelper(
    R defaultValue, const QString& path, const char* command,
    DefaultAction defaultAction, SocketAction socketAction)
{
    if (m_toolPath.isEmpty())
        return defaultAction();

    return socketAction();
}

template<typename DefaultAction>
qint64 RootTool::int64SingleArgCommandHelper(
    const QString& path, const char* command, DefaultAction defaultAction)
{
    return commandHelper(
        -1LL, path, command, defaultAction,
        [command, path, this]()
        {
            int64_t result = -1;
#if defined (Q_OS_LINUX)
            int socketPostfix = m_idHelper.take();
            execAndReadResult(
                socketPostfix,
                {command, path},
                [&result, socketPostfix]()
                {
                    return system_commands::domain_socket::readInt64(socketPostfix, &result);
                });
#endif
            return result;
        });
}

int RootTool::open(const QString& path, QIODevice::OpenMode mode)
{
#if defined (Q_OS_LINUX)
    int sysFlags = makeUnixOpenFlags(mode);
    if (m_toolPath.isEmpty())
        return SystemCommands().open(path.toStdString(), sysFlags, /*reportViaSocket*/ false);

    int fd = -1;
    int socketPostfix = m_idHelper.take();
    execAndReadResult(
        socketPostfix,
        {"open", path, QString::number(sysFlags)},
        [&fd, socketPostfix]()
        {
            fd = system_commands::domain_socket::readFd(socketPostfix);
            return fd > 0;
        });

    if (fd < 0)
        NX_WARNING(this, lm("Open failed for %1").args(path));

    return fd;
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
    return commandHelper(
        false, path, "exists",
        [path]() { return SystemCommands().isPathExists(path.toStdString(), /*reportViaSocket*/ false); },
        [path, this]()
        {
            int64_t result = (int64_t) false;
#if defined (Q_OS_LINUX)
            int socketPostfix = m_idHelper.take();
            execAndReadResult(
                socketPostfix,
                {"exists", path},
                [&result, socketPostfix]()
                {
                    return system_commands::domain_socket::readInt64(socketPostfix, &result);
                });
#endif
            return result;
        });
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

static QnAbstractStorageResource::FileInfoList fileListFromSerialized(const std::string& serializedList)
{
    QnAbstractStorageResource::FileInfoList result;
    StringRef stringRef;
    const char* data = serializedList.data();

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

template<typename DefaultAction, typename... Args>
std::string RootTool::stringCommandHelper(const char* command, DefaultAction action, Args&&... args)
{
    if (m_toolPath.isEmpty())
        return action();

#if defined(Q_OS_LINUX)
    std::string buf;
    int socketPostfix = m_idHelper.take();
    execAndReadResult(
        socketPostfix,
        {command, std::forward<Args>(args)...},
        [&buf, socketPostfix]()
        {
            return system_commands::domain_socket::readBuffer(
                socketPostfix, &readBufferReallocCallback, &buf);
        });
    return buf;
#else
    return "";
#endif
}

QnAbstractStorageResource::FileInfoList RootTool::fileList(const QString& path)
{
    return fileListFromSerialized(
        stringCommandHelper(
            "list",
            [path]()
            {
                return SystemCommands().serializedFileList(path.toStdString(), false);
            }, path));
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
            "devicePath",
            [fsPath]()
            {
                return SystemCommands().devicePath(fsPath.toStdString(), false);
            }, fsPath));

    return result;
}

/**
 * Serialized format is
 * |int partNumberLen|char* partNumberData|int serialNumberLen|char* serialNumberData|
 */
static bool dmiInfoFromSerialized(
    const std::string& serializedData, QString* outPartNumber, QString *outSerialNumber)
{
    if (serializedData.empty())
        return false;

    auto extractValue =
        [](const char** data, QString* outValue)
        {
            int len = *((int*) *data);
            *data += sizeof(std::string::size_type);
            *outValue = QString::fromLatin1(*data, len);
            *data += len;
        };

    auto data = serializedData.data();
    extractValue(&data, outPartNumber);
    extractValue(&data, outSerialNumber);

    return true;
}

bool RootTool::dmiInfo(QString* outPartNumber, QString *outSerialNumber)
{
    auto result = dmiInfoFromSerialized(
        stringCommandHelper(
            "dmiInfo",
            [=]()
            {
                return SystemCommands().serializedDmiInfo(false);
            }), outPartNumber, outSerialNumber);

    return result;
}

int RootTool::forkRoolTool(const std::vector<QString>& args)
{
#if defined(Q_OS_LINUX )
    const auto argsLine = makeArgsLine(args);
    const auto commandLine = makeCommand(m_toolPath, argsLine);

    std::vector<std::string> stringArgs;
    stringArgs.push_back(m_toolPath.toStdString());
    for (const auto& arg: args)
        stringArgs.push_back(arg.toStdString());

    std::vector<const char*> execArgs;
    for (const auto& stringArg: stringArgs)
        execArgs.push_back(stringArg.c_str());

    execArgs.push_back(nullptr);
    char** pdata = (char**) execArgs.data();
    char* exePath = (char*) malloc(m_toolPath.toStdString().size() + 1);
    memcpy(exePath, m_toolPath.toStdString().c_str(), m_toolPath.size() + 1);

    int childPid = ::fork();
    if (childPid < 0)
    {
        NX_WARNING(this, lm("Failed to fork with command %1").args(commandLine));
        free(exePath);
        return -1;
    }
    else if (childPid == 0)
    {

        execvp(exePath, pdata);
        NX_CRITICAL(false); /* If exec() was successful shouldn't get here. */
        return -1;
    }
    free(exePath);

    NX_VERBOSE(this, lm("Starting child %1 with command line %2").args(childPid, makeArgsLine(args)));

    NX_ASSERT(childPid > 0);
    return childPid;
#else
    NX_ASSERT(false, "Only linux is supported so far");
    return -1;
#endif
}

std::unique_ptr<RootTool> findRootTool(const QString& applicationPath)
{
    const auto toolPath = QFileInfo(applicationPath).dir().filePath("root_tool");
    bool isRootToolExists = QFileInfo(toolPath).exists();
    if (!isRootToolExists)
        NX_WARNING(typeid(RootTool), lm("Executable does not exist: %1").arg(toolPath));

#if defined (Q_OS_UNIX)
    isRootToolExists &= geteuid() != 0; //< No root_tool if the user is root
#endif

    printf("USING ROOT TOOL: %s\n", isRootToolExists ? "TRUE" : "FALSE");
    return std::make_unique<RootTool>(isRootToolExists ? toolPath : QString());
}

namespace detail {

int UniqueIdHelper::take() const
{
    QnMutexLocker lock(&m_mutex);
    int result = takeFirstAvailable();
    if (result != -1)
        return result;

    while (m_ids.count() == m_ids.size())
        m_condition.wait(lock.mutex());

    result = takeFirstAvailable();
    NX_ASSERT(result != -1);

    return result;
}

void UniqueIdHelper::putBack(int id)
{
    QnMutexLocker lock(&m_mutex);
    NX_ASSERT(m_ids.testBit(id));
    m_ids.setBit(id, false);
    m_condition.wakeAll();
}

int UniqueIdHelper::takeFirstAvailable() const
{
    for (int i = 0; i < m_ids.size(); ++i)
    {
        if (!m_ids.testBit(i))
        {
            m_ids.setBit(i);
            return i;
        }
    }

    return -1;
}

} // namespace detail

} // namespace mediaserver
} // namespace nx

