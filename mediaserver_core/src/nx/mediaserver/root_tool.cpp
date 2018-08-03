#include <type_traits>
#include <QFileInfo>
#include <QDir>
#include <utils/fs/file.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/system_error.h>
#include <nx/utils/raii_guard.h>
#if defined (Q_OS_LINUX)
    #include <nx/system_commands/domain_socket/read_linux.h>
    #include <nx/system_commands/domain_socket/send_linux.h>
#endif
#include "root_tool.h"

#if defined(Q_OS_WIN)
    #include <BaseTsd.h>
    #define ssize_t SSIZE_T
#endif

namespace nx {
namespace mediaserver {

namespace {
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

static int64_t int64ReceiveAction(int clientFd)
{
#if defined (Q_OS_LINUX)
    using namespace system_commands::domain_socket;
    int64_t result;
    readInt64(clientFd, &result);
    return result;
#else
    return -1;
#endif
}

static int fdReceiveAction(int clientFd)
{
#if defined (Q_OS_LINUX)
    using namespace system_commands::domain_socket;
    return readFd(clientFd);
#else
    return -1;
#endif
}

static void* readBufferReallocCallback(void* context, ssize_t size)
{
    std::string* buf = reinterpret_cast<std::string*>(context);
    buf->resize(size);
    return (void*) buf->data();
}

static std::string bufferReceiveAction(int clientFd)
{
#if defined (Q_OS_LINUX)
    using namespace system_commands::domain_socket;
    std::string result;
    readBuffer(clientFd, &readBufferReallocCallback, &result);
    return result;
#else
    return "";
#endif
}

template<typename ReceiveAction>
static auto execViaRootTool(const QString& command, ReceiveAction receiveAction) -> decltype(receiveAction(0))
{
    using RetType = decltype(receiveAction(0));

#if defined(Q_OS_LINUX)
    using namespace system_commands::domain_socket;
    int clientFd = createConnectedSocket(SystemCommands::kDomainSocket);
    if (clientFd == -1)
    {
        NX_WARNING(typeid(RootTool), lm("Failed to create client domain socket for command %1").args(command));
        return RetType();
    }

    sendBuffer(clientFd, command.toLatin1().constData(), command.toLatin1().size());
    auto result = receiveAction(clientFd);

    ::close(clientFd);

    return result;
#else
    return RetType();
#endif
}

static QString enquote(const QString& s)
{
    return "'" + s + "'";
}

} // namespace

RootTool::RootTool(const QString& toolPath): m_toolPath(toolPath)
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
            password.empty() ? none : boost::optional<std::string>(password));

        logMountResult(mountResult, false);
        return mountResultToStorageInitResult(mountResult);
    }

    QString commandString = "mount " + enquote(uncString) + " " +  enquote(path);

    if (!url.userName().isEmpty())
        commandString += " " + url.userName();

    if (!url.password().isEmpty())
        commandString += " " + url.password();

    return mountResultToStorageInitResult(
        (SystemCommands::MountCode) execViaRootTool(commandString, &int64ReceiveAction));
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
    if (m_toolPath.isEmpty())
        return SystemCommands().unmount(path.toStdString());

    return (SystemCommands::UnmountCode) execViaRootTool("unmount " + enquote(path), &int64ReceiveAction);
#else
    return SystemCommands::UnmountCode::noPermissions;
#endif
}

bool RootTool::changeOwner(const QString& path, bool isRecursive)
{
    int uid = 0;
    int gid = 0;

#if defined (Q_OS_LINUX)
    uid = getuid();
    gid = getgid();
#endif

    if (m_toolPath.isEmpty())
        return SystemCommands().changeOwner(path.toStdString(), uid, gid, isRecursive);

    return (bool) execViaRootTool(
        "chown " + enquote(path) + " " + QString::number(uid) + " " + QString::number(gid) +
            " " + (isRecursive ? " recursive" : ""),
        &int64ReceiveAction);
}

bool RootTool::makeDirectory(const QString& path)
{
    if (m_toolPath.isEmpty())
        return SystemCommands().makeDirectory(path.toStdString());

    return (bool) execViaRootTool("mkdir " + enquote(path), &int64ReceiveAction);
}

bool RootTool::removePath(const QString& path)
{
    if (m_toolPath.isEmpty())
        return SystemCommands().removePath(path.toStdString());

    return (bool) execViaRootTool("rm " + enquote(path), &int64ReceiveAction);
}

bool RootTool::rename(const QString& oldPath, const QString& newPath)
{
    if (m_toolPath.isEmpty())
        return SystemCommands().rename(oldPath.toStdString(), newPath.toStdString());

    return (bool) execViaRootTool("mv " + enquote(oldPath) + " " + enquote(newPath), &int64ReceiveAction);
}

int RootTool::open(const QString& path, QIODevice::OpenMode mode)
{
    int sysFlags = 0;

#if defined (Q_OS_LINUX)
    sysFlags = makeUnixOpenFlags(mode);
#endif

    if (m_toolPath.isEmpty())
        return SystemCommands().open(path.toStdString(), sysFlags);

    return (int) execViaRootTool("open " + enquote(path )+ " " + QString::number(sysFlags), &fdReceiveAction);
}

qint64 RootTool::freeSpace(const QString& path)
{
    if (m_toolPath.isEmpty())
        return SystemCommands().freeSpace(path.toStdString());

    return execViaRootTool("freeSpace " + enquote(path), &int64ReceiveAction);
}

qint64 RootTool::totalSpace(const QString& path)
{
    if (m_toolPath.isEmpty())
        return SystemCommands().totalSpace(path.toStdString());

    return execViaRootTool("totalSpace " + enquote(path), &int64ReceiveAction);
}

bool RootTool::isPathExists(const QString& path)
{
    if (m_toolPath.isEmpty())
        return SystemCommands().isPathExists(path.toStdString());

    return (bool) execViaRootTool("exists " + enquote(path), &int64ReceiveAction);
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

QnAbstractStorageResource::FileInfoList RootTool::fileList(const QString& path)
{
    if (m_toolPath.isEmpty())
        return fileListFromSerialized(SystemCommands().serializedFileList(path.toStdString()));

    return fileListFromSerialized(execViaRootTool("list " + enquote(path), &bufferReceiveAction));
}

qint64 RootTool::fileSize(const QString& path)
{
    if (m_toolPath.isEmpty())
        return SystemCommands().fileSize(path.toStdString());

    return execViaRootTool("size " + enquote(path), &int64ReceiveAction);
}

QString RootTool::devicePath(const QString& fsPath)
{
    if (m_toolPath.isEmpty())
        return QString::fromStdString(SystemCommands().devicePath(fsPath.toStdString()));

    return QString::fromStdString(execViaRootTool("devicePath " + enquote(fsPath), &bufferReceiveAction));
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
    if (m_toolPath.isEmpty())
    {
        return dmiInfoFromSerialized(SystemCommands().serializedDmiInfo(), outPartNumber,
            outSerialNumber);
    }

    return dmiInfoFromSerialized(execViaRootTool("dmiInfo", &bufferReceiveAction), outPartNumber,
        outSerialNumber);
}

std::unique_ptr<RootTool> findRootTool(const QString& applicationPath)
{
    const auto toolPath = QFileInfo(applicationPath).dir().filePath("root_tool");
    const auto alternativeToolPath = QFileInfo(applicationPath).dir().filePath("root-tool-bin");
    bool isRootToolExists = QFileInfo(toolPath).exists() || QFileInfo(alternativeToolPath).exists();
    if (!isRootToolExists)
        NX_WARNING(typeid(RootTool), lm("Executable does not exist: %1").arg(toolPath));

#if defined (Q_OS_UNIX)
    isRootToolExists &= geteuid() != 0; //< No root_tool if the user is root
#endif

    printf("USING ROOT TOOL: %s\n", isRootToolExists ? "TRUE" : "FALSE");
    return std::make_unique<RootTool>(isRootToolExists ? toolPath : QString());
}

} // namespace mediaserver
} // namespace nx

