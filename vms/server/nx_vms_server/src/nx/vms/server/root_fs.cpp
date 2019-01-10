#include <type_traits>
#include <QFileInfo>
#include <QDir>
#include <utils/fs/file.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/system_error.h>
#include <nx/utils/scope_guard.h>
#include <media_server/media_server_module.h>
#if defined (Q_OS_LINUX)
    #include <nx/system_commands/domain_socket/read_linux.h>
    #include <nx/system_commands/domain_socket/send_linux.h>
#endif
#include "root_fs.h"

#if defined(Q_OS_WIN)
    #include <BaseTsd.h>
    #define ssize_t SSIZE_T
#endif

namespace nx {
namespace vms::server {

namespace {

static int64_t receiveInt64Action(int clientFd)
{
    #if defined(Q_OS_LINUX)
        using namespace system_commands::domain_socket;
        int64_t result;
        readInt64(clientFd, &result);
        return result;
    #else
        return -1;
    #endif
}

static int receiveFdAction(int clientFd)
{
    #if defined(Q_OS_LINUX)
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

static std::string receiveBufferAction(int clientFd)
{
    #if defined(Q_OS_LINUX)
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
            NX_WARNING(typeid(RootFileSystem),
                lm("Failed to create client domain socket for command %1").args(command));
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

RootFileSystem::RootFileSystem(bool useTool): m_ignoreTool(!useTool)
{
}

Qn::StorageInitResult RootFileSystem::mount(const QUrl& url, const QString& path)
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
                            url, path, viaString));
                    break;
                }
            };

        using namespace boost;

        auto uncString = "//" + url.host() + url.path();
        if (m_ignoreTool)
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
            (SystemCommands::MountCode) execViaRootTool(commandString, &receiveInt64Action));
    #else
        return Qn::StorageInitResult::StorageInit_WrongPath;
    #endif
}

Qn::StorageInitResult RootFileSystem::remount(const QUrl& url, const QString& path)
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

SystemCommands::UnmountCode RootFileSystem::unmount(const QString& path)
{
    if (m_ignoreTool)
        return SystemCommands().unmount(path.toStdString());

    return (SystemCommands::UnmountCode) execViaRootTool("unmount " + enquote(path),
        &receiveInt64Action);
}

bool RootFileSystem::changeOwner(const QString& path, bool isRecursive)
{
    int userId = 0;
    int groupId = 0;

    #if defined(Q_OS_LINUX)
        userId = getuid();
        groupId = getgid();
    #endif

    if (m_ignoreTool)
        return SystemCommands().changeOwner(path.toStdString(), userId, groupId, isRecursive);

    return (bool) execViaRootTool(
        "chown " + enquote(path) + " " + QString::number(userId) + " " + QString::number(groupId) +
            " " + (isRecursive ? " recursive" : ""),
        &receiveInt64Action);
}

bool RootFileSystem::makeDirectory(const QString& path)
{
    if (m_ignoreTool)
        return SystemCommands().makeDirectory(path.toStdString());

    return (bool) execViaRootTool("mkdir " + enquote(path), &receiveInt64Action);
}

bool RootFileSystem::removePath(const QString& path)
{
    if (m_ignoreTool)
        return SystemCommands().removePath(path.toStdString());

    return (bool) execViaRootTool("rm " + enquote(path), &receiveInt64Action);
}

bool RootFileSystem::rename(const QString& oldPath, const QString& newPath)
{
    if (m_ignoreTool)
        return SystemCommands().rename(oldPath.toStdString(), newPath.toStdString());

    return (bool) execViaRootTool("mv " + enquote(oldPath) + " " + enquote(newPath),
        &receiveInt64Action);
}

int RootFileSystem::open(const QString& path, QIODevice::OpenMode mode)
{
    int sysFlags = 0;

    #if defined(Q_OS_LINUX)
        sysFlags = makeUnixOpenFlags(mode);
    #endif

    if (m_ignoreTool)
        return SystemCommands().open(path.toStdString(), sysFlags);

    return (int) execViaRootTool("open " + enquote(path )+ " " + QString::number(sysFlags), &receiveFdAction);
}

qint64 RootFileSystem::freeSpace(const QString& path)
{
    if (m_ignoreTool)
        return SystemCommands().freeSpace(path.toStdString());

    return execViaRootTool("freeSpace " + enquote(path), &receiveInt64Action);
}

qint64 RootFileSystem::totalSpace(const QString& path)
{
    if (m_ignoreTool)
        return SystemCommands().totalSpace(path.toStdString());

    return execViaRootTool("totalSpace " + enquote(path), &receiveInt64Action);
}

bool RootFileSystem::isPathExists(const QString& path)
{
    if (m_ignoreTool)
        return SystemCommands().isPathExists(path.toStdString());

    return (bool) execViaRootTool("exists " + enquote(path), &receiveInt64Action);
}

static SystemCommands::Stats statsFromSerialized(const std::string& buffer)
{
    SystemCommands::Stats result;
    memcpy(&result, buffer.data(), sizeof(SystemCommands::Stats));
    return result;
}

SystemCommands::Stats RootFileSystem::stat(const QString& path)
{
    if (m_ignoreTool)
        return SystemCommands().stat(path.toStdString());

    return statsFromSerialized(execViaRootTool("stat " + enquote(path), &receiveBufferAction));
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

QnAbstractStorageResource::FileInfoList RootFileSystem::fileList(const QString& path)
{
    if (m_ignoreTool)
        return fileListFromSerialized(SystemCommands().serializedFileList(path.toStdString()));

    return fileListFromSerialized(execViaRootTool("list " + enquote(path), &receiveBufferAction));
}

qint64 RootFileSystem::fileSize(const QString& path)
{
    if (m_ignoreTool)
        return SystemCommands().fileSize(path.toStdString());

    return execViaRootTool("size " + enquote(path), &receiveInt64Action);
}

QString RootFileSystem::devicePath(const QString& fsPath)
{
    if (m_ignoreTool)
        return QString::fromStdString(SystemCommands().devicePath(fsPath.toStdString()));

    return QString::fromStdString(execViaRootTool("devicePath " + enquote(fsPath), &receiveBufferAction));
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

bool RootFileSystem::dmiInfo(QString* outPartNumber, QString *outSerialNumber)
{
    if (m_ignoreTool)
    {
        return dmiInfoFromSerialized(SystemCommands().serializedDmiInfo(), outPartNumber,
            outSerialNumber);
    }

    return dmiInfoFromSerialized(execViaRootTool("dmiInfo", &receiveBufferAction), outPartNumber,
        outSerialNumber);
}

std::unique_ptr<RootFileSystem> instantiateRootFileSystem(
    bool isRootToolEnabled,
    const QString& applicationPath)
{
    const auto toolPath = QFileInfo(applicationPath).dir().filePath("root_tool");
    const auto alternativeToolPath = QFileInfo(applicationPath).dir().filePath("root-tool-bin");
    bool isRootToolExists = QFileInfo(toolPath).exists() || QFileInfo(alternativeToolPath).exists();
    bool isRootToolUsed = isRootToolExists && isRootToolEnabled;

    #if defined (Q_OS_UNIX)
        isRootToolUsed &= geteuid() != 0; //< No root_tool if the user is root
    #endif

    NX_INFO(typeid(RootFileSystem), lm("Root tool enabled: %1").args(isRootToolUsed));
    return std::make_unique<RootFileSystem>(isRootToolUsed);
}

} // namespace vms::server
} // namespace nx

