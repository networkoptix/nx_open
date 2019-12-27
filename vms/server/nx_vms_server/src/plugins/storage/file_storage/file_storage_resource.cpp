
#include <iostream>
#include <array>

#include <QtCore/QDir>

#include "file_storage_resource.h"
#include <nx/utils/log/log.h>
#include "utils/common/util.h"
#include "utils/common/buffered_file.h"
#include "recorder/file_deletor.h"
#include "utils/fs/file.h"
#include <common/common_module.h>
#include <utils/common/writer_pool.h>
#include <nx/fusion/serialization/lexical.h>
#include <media_server/media_server_module.h>
#include <nx/system_commands.h>
#include <nx/vms/server/root_fs.h>
#include <nx/vms/server/fs/media_paths/media_paths.h>
#include <nx/vms/server/fs/media_paths/media_paths_filter_config.h>
#include <nx/utils/app_info.h>

#ifndef _WIN32
#   include <platform/monitoring/global_monitor.h>
#   include <platform/platform_abstraction.h>
#endif

#ifdef Q_OS_WIN
#include "windows.h"
#endif
#include <media_server/settings.h>
#include <recorder/storage_manager.h>

#ifndef _WIN32
#   include <sys/types.h>
#   include <sys/stat.h>
#   include <fcntl.h>
#   include <unistd.h>
#   include <errno.h>
#   include <dirent.h>
#endif

#ifdef WIN32
#pragma comment(lib, "mpr.lib")
#endif

#include <sstream>
#include <cstdlib>
#include <ctime>

#ifndef Q_OS_WIN
    const QString QnFileStorageResource::FROM_SEP = lit("\\");
    const QString QnFileStorageResource::TO_SEP = lit("/");
#else
    const QString QnFileStorageResource::FROM_SEP = lit("/");
    const QString QnFileStorageResource::TO_SEP = lit("\\");
#endif

#ifdef __APPLE__
static const auto MS_NODEV = MNT_NODEV;
static const auto MS_NOSUID = MNT_NOSUID;
static const auto MS_NOEXEC = MNT_NOEXEC;
#endif

namespace {

const qint64 kMaxNasStorageSpaceLimit = 100ll * 1024 * 1024 * 1024; // 100 Gb
const qint64 kMaxLocalStorageSpaceLimit = 30ll * 1024 * 1024 * 1024; // 30 Gb
const int kMaxSpaceLimitRatio = 10; // i.e. max space limit <= totalSpace / 10

using PlatformMonitor = nx::vms::server::PlatformMonitor;

#if defined(Q_OS_WIN)
static QString getDevicePath(QnMediaServerModule* /*serverModule*/, const QString& path)
{
    return path;
}

static QString sysDrivePath(QnMediaServerModule* /*serverModule*/)
{
    static QString deviceString;

    if (!deviceString.isNull())
        return deviceString;

    const DWORD bufSize = MAX_PATH + 1;
    TCHAR buf[bufSize];
    GetWindowsDirectory(buf, bufSize);

    deviceString = QString::fromWCharArray(buf, bufSize).left(2);

    return deviceString;
}

#elif defined(Q_OS_LINUX)

static QString getDevicePath(QnMediaServerModule* serverModule, const QString& path)
{
    return serverModule->rootFileSystem()->devicePath(path);
}

static QString sysDrivePath(QnMediaServerModule* serverModule)
{
    return getDevicePath(serverModule, "/");
}

#else // Unsupported OS so far

static const QString& sysDrivePath()
{
    return QString();
}

#endif

QString genLocalPath(const QString &url, const QString &prefix = "/tmp/")
{
    return prefix + QnFileStorageResource::tempFolderName() + QString::number(qHash(url), 16);
}

} // namespace <anonymous>


QIODevice* QnFileStorageResource::open(
    const QString& fileName, QIODevice::OpenMode openMode, int bufferSize)
{
    auto sourceIoDevice = openInternal(fileName, openMode, bufferSize);
    if (!sourceIoDevice)
        return nullptr;
    return wrapIoDevice(std::unique_ptr<QIODevice>(sourceIoDevice));
}

QIODevice* QnFileStorageResource::openInternal(const QString& fileName, QIODevice::OpenMode openMode)
{
    return openInternal(fileName, openMode, 0);
}

QIODevice* QnFileStorageResource::openInternal(
    const QString& url,
    QIODevice::OpenMode openMode,
    int bufferSize)
{
    if (!isValid())
        return nullptr;

    QString fileName = removeProtocolPrefix(translateUrlToLocal(url));

    int ioBlockSize = 0;
    int ffmpegBufferSize = 0;

    int ffmpegMaxBufferSize =
        serverModule()->settings().maxFfmpegBufferSize();

    int systemFlags = 0;
    if (openMode & QIODevice::WriteOnly)
    {
        ioBlockSize = serverModule()->settings().ioBlockSize();

        ffmpegBufferSize = qMax(
            serverModule()->settings().ffmpegBufferSize(),
            bufferSize);

#ifdef Q_OS_WIN
        if ((openMode & QIODevice::ReadWrite) == QIODevice::ReadWrite)
            systemFlags = 0;
        else if (!serverModule()->settings().disableDirectIO())
            systemFlags = FILE_FLAG_NO_BUFFERING;
#endif
    }

    if (openMode.testFlag(QIODevice::Unbuffered))
        ioBlockSize = ffmpegBufferSize = 0;

#if defined (Q_OS_WIN)
    std::unique_ptr<QBufferedFile> rez(
        new QBufferedFile(
            std::shared_ptr<IQnFile>(new QnFile(fileName)),
            ioBlockSize,
            ffmpegBufferSize,
            ffmpegMaxBufferSize,
            getId()));
    if (rez)
    {
        rez->setSystemFlags(systemFlags);
        rez->open(openMode);
    }
    return rez.release();

#elif defined(Q_OS_UNIX)

    int fd = rootTool()->open(fileName, openMode);
    if (fd < 0)
    {
        if (openMode & QIODevice::WriteOnly && rootTool()->makeDirectory(QnFile::absolutePath(fileName)))
            fd = rootTool()->open(fileName, openMode);

        if (fd < 0)
        {
            NX_ERROR(this, lm("[open] failed to open file %1").args(fileName));
            return nullptr;
        }
    }

    auto result = new QBufferedFile(
        std::make_shared<QnFile>(fd), ioBlockSize, ffmpegBufferSize, ffmpegMaxBufferSize, getId());
    result->open(openMode);

    return result;
#endif

    return nullptr;
}

nx::vms::server::RootFileSystem* QnFileStorageResource::rootTool() const
{
    return serverModule()->rootFileSystem();
}

void QnFileStorageResource::setLocalPathSafe(const QString &path)
{
    QnMutexLocker lk(&m_mutex);
    m_localPath = path;
}


QString QnFileStorageResource::getLocalPathSafe() const
{
    QnMutexLocker lk(&m_mutex);
    return m_localPath;
}

QString QnFileStorageResource::getPath() const
{
    QString url = getUrl();
    if (!url.contains(lit("://")))
        return url;
    else
        return QUrl(url).path();
}

qint64 QnFileStorageResource::getDeviceSizeByLocalPossiblyNonExistingPath(const QString &path) const
{
    qint64 result;

    if (serverModule()->rootFileSystem()->isPathExists(path))
        return serverModule()->rootFileSystem()->totalSpace(path);

    if (!serverModule()->rootFileSystem()->makeDirectory(path))
        return -1;

    result = serverModule()->rootFileSystem()->totalSpace(path);
    serverModule()->rootFileSystem()->removePath(path);

    return result;
}

void QnFileStorageResource::setIsSystemFlagIfNeeded()
{
    if (*m_isSystem.lock())
        return;

    const QString sysPath = sysDrivePath(m_serverModule);
    const bool isSystem = !sysPath.isNull() && getDevicePath(m_serverModule, getUrl()).startsWith(sysPath);
    NX_DEBUG(
        this,
        "Setting isSystem flag for storage '%1'. System drive path: '%2'. Result: %3",
        nx::utils::url::hidePassword(getUrl()), sysPath, isSystem);

    *m_isSystem.lock() = isSystem;
}

Qn::StorageInitResult QnFileStorageResource::initStorageDirectory(const QString& url)
{
    if (rootTool()->isPathExists(url))
    {
        NX_DEBUG(this, "[initOrUpdate] Storage directory '%1' exists", url);
        return Qn::StorageInit_Ok;
    }

    if (!rootTool()->makeDirectory(url))
    {
        NX_WARNING(this, "[initOrUpdate] storage directory '%1' doesn't exist and mkdir failed", url);
        return Qn::StorageInit_WrongPath;
    }

    NX_DEBUG(this, "[initOrUpdate] storage directory '%1' was successfully created", url);
    return Qn::StorageInit_Ok;
}

Qn::StorageInitResult QnFileStorageResource::initRemoteStorage(const QString& url)
{
    if (!nx::utils::Url(url).isValid())
    {
        NX_WARNING(
            this, "[initOrUpdate] Storage url '%1' is not valid. Won't try mounting",
            nx::utils::url::hidePassword(url));

        return Qn::StorageInit_WrongPath;
    }

    const auto result = mountTmpDrive(url);
    if (result != Qn::StorageInit_Ok)
    {
         NX_WARNING(
            this, "[initOrUpdate] Failed to mount remote storage '%1'",
            nx::utils::url::hidePassword(url));
    }
    else
    {
         NX_DEBUG(
            this, "[initOrUpdate] Remote storage '%1' successfully initialized",
            nx::utils::url::hidePassword(url));
    }

    return result;
}

bool QnFileStorageResource::isValid() const
{
    return m_state == Qn::StorageInit_Ok;
}

Qn::StorageInitResult QnFileStorageResource::initOrUpdateInternal()
{
    NX_VERBOSE(this, "[initOrUpdate] for storage %1 begin", nx::utils::url::hidePassword(getUrl()));

    QString url = getUrl();
    if (!NX_ASSERT(!url.isEmpty()))
    {
        NX_WARNING(this, "[initOrUpdate] storage url is empty");
        return Qn::StorageInit_WrongPath;
    }

    Qn::StorageInitResult result = isValid()
        ? checkMountedStatus()
        : (url.contains("://") ? initRemoteStorage(url) : initStorageDirectory(url));
    if (result != Qn::StorageInit_Ok)
        return result;

    return testWrite();
}

bool QnFileStorageResource::isSystem() const
{
    QnMutexLocker lock(&m_mutex);
    const auto lockedIsSystem = *m_isSystem.lock();
    return lockedIsSystem ? *lockedIsSystem : false;
}

bool QnFileStorageResource::checkWriteCap() const
{
    return isValid();
}

bool QnFileStorageResource::checkDBCap() const
{
    if (!isValid())
        return false;

#ifdef _WIN32
    return true;
#else
    // if storage is mounted via mediaserver (smb://...)
    // let's not create media DB there
    if (!getLocalPathSafe().isEmpty())
        return false;

    {
        QnMutexLocker lk(&m_mutex);
        if (m_dbReady.is_initialized())
            return *m_dbReady;
    }

    // Same for mounted by hand remote storages (NAS)
    QList<PlatformMonitor::PartitionSpace> partitions =
        serverModule()->platform()->monitor()->
            PlatformMonitor::totalPartitionSpaceInfo(
            PlatformMonitor::NetworkPartition);

    bool dbReady = true;
    for(const PlatformMonitor::PartitionSpace& partition: partitions)
    {
        if(getPath().startsWith(partition.path))
            dbReady = false;
    }

    {
        QnMutexLocker lk(&m_mutex);
        m_dbReady = dbReady;
        return dbReady;
    }
#endif
}

int QnFileStorageResource::getCapabilities() const
{
    if (!isValid())
        return 0;

    if (checkDBCap())
    {
        QnMutexLocker lk(&m_mutex);
        m_capabilities |= QnAbstractStorageResource::cap::DBReady;
    }

    int writeCap = checkWriteCap() ? QnAbstractStorageResource::cap::WriteFile : 0;
    {
        QnMutexLocker lk(&m_mutex);
        return m_capabilities | writeCap;
    }
    return 0;
}

QString QnFileStorageResource::translateUrlToLocal(const QString &url) const
{
    QnMutexLocker lk(&m_mutex);

    if (m_localPath.isEmpty())
        return url;
    else
    {
        QString storagePath = QUrl(getUrl()).path().replace(FROM_SEP, TO_SEP);
        QString tmpPath = QUrl(url).path().replace(FROM_SEP, TO_SEP);
        if (storagePath == tmpPath)
            tmpPath.clear();
        else
            tmpPath = tmpPath.mid(storagePath.size());
        tmpPath = m_localPath + tmpPath;
        return tmpPath;
    }
}

QString QnFileStorageResource::translateUrlToRemote(const QString &url) const
{
    QnMutexLocker lk(&m_mutex);

    if (m_localPath.isEmpty())
        return url;
    else
    {
        QString ret = getUrl() + url.mid(m_localPath.size());
        return ret;
    }
}

void QnFileStorageResource::removeOldDirs(QnMediaServerModule* serverModule)
{
#ifndef _WIN32

    const QString prefix = lit("/tmp/") + tempFolderName();
    const QFileInfoList tmpEntries = QDir("/tmp").entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot);

    for (const QFileInfo &entry: tmpEntries)
    {
        if (entry.absoluteFilePath().indexOf(prefix) == -1)
            continue;

        nx::SystemCommands::UnmountCode result =
                serverModule->rootFileSystem()->unmount(entry.absoluteFilePath());

        NX_VERBOSE(
            typeid(QnFileStorageResource),
            lm("[mount, removeOldDirs] Unmounting temporary directory %1, result: %2")
                .args(entry.absoluteFilePath(), nx::SystemCommands::unmountCodeToString(result)));

        switch (result)
        {
            case nx::SystemCommands::UnmountCode::ok:
            case nx::SystemCommands::UnmountCode::notMounted:
                if (!serverModule->rootFileSystem()->removePath(entry.absoluteFilePath()))
                {
                    NX_ERROR(typeid(QnFileStorageResource),
                        lm("[removeOldDirs] Remove %1 failed").args(entry.absoluteFilePath()));
                }
                break;
            case nx::SystemCommands::UnmountCode::busy:
                NX_WARNING(typeid(QnFileStorageResource),
                    lm("[mount, removeOldDirs] Won't remove %1 since resource is busy")
                        .args(entry.absoluteFilePath()));
                break;
            case nx::SystemCommands::UnmountCode::noPermissions:
                NX_WARNING(typeid(QnFileStorageResource),
                    lm("[mount, removeOldDirs] NO permissions to remove %1")
                        .args(entry.absoluteFilePath()));
                break;
            case nx::SystemCommands::UnmountCode::notExists:
                NX_VERBOSE(typeid(QnFileStorageResource),
                    lm("[mount, removeOldDirs] Won't remove %1 since it doesn't exist")
                        .args(entry.absoluteFilePath()));
                break;
        }
    }

#endif
}

#if !defined(Q_OS_WIN)

Qn::StorageInitResult QnFileStorageResource::mountTmpDrive(const QString& url)
{
    QString localPath = genLocalPath(url);
    setLocalPathSafe(localPath);

    return rootTool()->remount(QUrl(url), localPath);
}

#else

Qn::StorageInitResult QnFileStorageResource::updatePermissionsHelper(
    LPWSTR userName,
    LPWSTR password,
    NETRESOURCE* netRes) const
{
    DWORD errCode = WNetUseConnection(
        0,   // window handler, not used
        netRes,
        password,
        userName,
        0,   // connection flags, should work with just 0 though
        0,
        0,   // additional connection info buffer size
        NULL // additional connection info buffer
    );

    auto logAndExit = [this] (const char* message, Qn::StorageInitResult result)
    {
        NX_DEBUG(this, lit("%1 Mounting remote drive %2. Result: %3")
               .arg(Q_FUNC_INFO)
               .arg(nx::utils::url::hidePassword(getUrl()))
               .arg(message));
        return result;
    };

#define STR(x) #x

    switch (errCode)
    {
        case NO_ERROR: return logAndExit(STR(NO_ERROR), Qn::StorageInit_Ok);
        case ERROR_SESSION_CREDENTIAL_CONFLICT: return logAndExit(STR(ERROR_SESSION_CREDENTIAL_CONFLICT), Qn::StorageInit_Ok);
        case ERROR_ALREADY_ASSIGNED: return logAndExit(STR(ERROR_ALREADY_ASSIGNED), Qn::StorageInit_Ok);
        case ERROR_ACCESS_DENIED: return logAndExit(STR(ERROR_ACCESS_DENIED), Qn::StorageInit_WrongAuth);
        case ERROR_WRONG_PASSWORD: return logAndExit(STR(ERROR_WRONG_PASSWORD), Qn::StorageInit_WrongAuth);
        case ERROR_INVALID_PASSWORD: return logAndExit(STR(ERROR_INVALID_PASSWORD), Qn::StorageInit_WrongAuth);

        default:
            NX_WARNING(this, lit("%1 Mounting remote drive %2 error %3.")
                    .arg(Q_FUNC_INFO)
                    .arg(nx::utils::url::hidePassword(getUrl()))
                    .arg(errCode));
            return Qn::StorageInit_WrongPath;
    };

#undef STR

    return Qn::StorageInit_WrongPath;
}

struct NetResHolder
{
    NetResHolder(const nx::utils::Url& url): m_path(constructPath(url))
    {
        memset(&m_netRes, 0, sizeof(m_netRes));
        m_netRes.dwType = RESOURCETYPE_DISK;
        m_netRes.lpRemoteName = (LPWSTR) m_path.constData();
    }

    NETRESOURCE* operator*() { return &m_netRes; }

private:
    QString m_path;
    NETRESOURCE m_netRes;

    static QString constructPath(const nx::utils::Url& url)
    {
        return "\\\\" + url.host() + "\\" + url.path().mid((1));
    }
};

Qn::StorageInitResult QnFileStorageResource::updatePermissions(const QString& url) const
{
    if (!url.startsWith("smb://"))
        return Qn::StorageInit_Ok;

    NX_VERBOSE(this, "Mounting remote drive %2", nx::utils::url::hidePassword(url));
    nx::utils::Url storageUrl(url);
    QString initialUserName = storageUrl.userName().isEmpty() ? "guest" : storageUrl.userName();
    std::array<QString, 2> userNamesToTest = {
        initialUserName,
        lit("WORKGROUP\\") + initialUserName,
    };

    const auto password = storageUrl.password();
    bool wrongAuth = false;
    NetResHolder netRes(storageUrl);

    for (const auto& userName : userNamesToTest)
    {
        auto result = updatePermissionsHelper(
            (LPWSTR) userName.constData(), (LPWSTR) password.constData(), *netRes);
        switch (result)
        {
            case Qn::StorageInit_Ok:
                return result;
            case Qn::StorageInit_WrongAuth:
                wrongAuth = true;
                break;
        };
    }

    return wrongAuth ? Qn::StorageInit_WrongAuth : Qn::StorageInit_WrongPath;
}

Qn::StorageInitResult QnFileStorageResource::mountTmpDrive(const QString& url)
{
    nx::utils::Url u(url);
    setLocalPathSafe("\\\\" + u.host() + u.path().replace("/", "\\"));
    return updatePermissions(url);
}
#endif

void QnFileStorageResource::setUrl(const QString& url)
{
    QnStorageResource::setUrl(url);
    m_state = Qn::StorageInit_CreateFailed;
}

QnFileStorageResource::QnFileStorageResource(QnMediaServerModule* serverModule):
    base_type(serverModule),
    m_serverModule(serverModule)
{
    m_capabilities |= QnAbstractStorageResource::cap::RemoveFile;
    m_capabilities |= QnAbstractStorageResource::cap::ListFile;
    m_capabilities |= QnAbstractStorageResource::cap::ReadFile;
}

QnFileStorageResource::~QnFileStorageResource()
{
    const auto localPath = getLocalPathSafe();
    if (localPath.isEmpty())
        return;

    #if defined(Q_OS_WIN)
        DWORD result = WNetCancelConnection((LPWSTR) localPath.constData(), TRUE);
        NX_VERBOSE(this, "Cancelling NET connection %1, result: %2", localPath, result);
    #else
        #if __linux__
            auto result = rootTool()->unmount(localPath);
            NX_VERBOSE(
                this, "Unmounting folder %1 while destructing object result: %2",
                localPath, nx::SystemCommands::unmountCodeToString(result));
        #elif __APPLE__
            unmount(localPath.toLatin1().constData(), 0);
        #endif
            rmdir(localPath.toLatin1().constData());
    #endif
}

QString QnFileStorageResource::tempFolderName()
{
    return nx::utils::AppInfo::brand() + "_temp_folder_";
}

bool QnFileStorageResource::removeFile(const QString& url)
{
    if (!isValid())
        return false;

    const auto fileName = removeProtocolPrefix(translateUrlToLocal(url));
    if (serverModule()->rootFileSystem()->removePath(fileName))
        return true;
    return !serverModule()->rootFileSystem()->isPathExists(fileName);
}

bool QnFileStorageResource::renameFile(const QString& oldName, const QString& newName)
{
    if (!isValid())
        return false;

    const auto oldPath = translateUrlToLocal(oldName);
    const auto newPath = translateUrlToLocal(newName);
    if (rootTool()->rename(oldPath, newPath))
        return true;

    NX_ERROR(this, lm("Rename %1 to %2 failed").args(oldName, newName));
    return false;
}

bool QnFileStorageResource::removeDir(const QString& url)
{
    if (!isValid())
        return false;

    const auto path = removeProtocolPrefix(translateUrlToLocal(url));
    if (rootTool()->removePath(path))
        return true;

    NX_ERROR(this, lm("removeDir failed for %1").args(path));
    return false;
}

bool QnFileStorageResource::isDirExists(const QString& url)
{
    if (!isValid())
        return false;

    QString path = removeProtocolPrefix(translateUrlToLocal(url));
    return rootTool()->isPathExists(path);
}

bool QnFileStorageResource::isFileExists(const QString& url)
{
    if (!isValid())
        return false;

    QString path = removeProtocolPrefix(translateUrlToLocal(url));
    return rootTool()->isPathExists(path);
}

qint64 QnFileStorageResource::getFreeSpace()
{
    QString localPathCopy = getLocalPathSafe();

    if (!isValid())
        return QnStorageResource::kUnknownSize;

    QString path = localPathCopy.isEmpty() ?  getPath() : localPathCopy;
    return rootTool()->freeSpace(path);
}

qint64 QnFileStorageResource::getTotalSpace() const
{
    if (!isValid())
        return QnStorageResource::kUnknownSize;

    return m_cachedTotalSpace;
}

QnAbstractStorageResource::FileInfoList QnFileStorageResource::getFileList(const QString& dirName)
{
    if (!isValid())
        return QnAbstractStorageResource::FileInfoList();

    QnAbstractStorageResource::FileInfoList ret;

    QString path = translateUrlToLocal(dirName);
    QDir dir(path);
    if (!dir.exists())
    {
#if defined (Q_OS_UNIX)
        return rootTool()->fileList(path);
#endif
        return ret;
    }

    QFileInfoList localList = dir.entryInfoList(
        QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot,
        QDir::DirsFirst
    );


    for (const QFileInfo &entry : localList)
    {
        ret.append(
            QnAbstractStorageResource::FileInfo(
                translateUrlToRemote(entry.absoluteFilePath()),
                entry.size(),
                entry.isDir()));
    }
    return ret;
}

qint64 QnFileStorageResource::getFileSize(const QString& url) const
{
    if (!isValid())
        return -1;

    return rootTool()->fileSize(translateUrlToLocal(url));
}

bool QnFileStorageResource::testWriteCapInternal() const
{
    QString fileName(lit("%1%2.tmp"));
    QString localGuid = commonModule()->moduleGUID().toString();
    localGuid = localGuid.mid(1, localGuid.length() - 2);
    fileName = fileName.arg(closeDirPath(translateUrlToLocal(getPath()))).arg(localGuid);

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly))
        return true;

#if defined(Q_OS_UNIX)
    int rootToolFd = rootTool()->open(fileName, QIODevice::WriteOnly);
    if (rootToolFd > 0)
    {
        ::close(rootToolFd);
        return true;
    }
#endif

    NX_ERROR(
        this, lm("[initOrUpdate, WriteTest] Open file %1 for writing failed").args(fileName));

    return false;
}

Qn::StorageInitResult QnFileStorageResource::checkMountedStatus() const
{
    #if defined (Q_OS_WIN)
        if (isExternal())
            return Qn::StorageInit_Ok; //< #TODO #akulikov Implement real check by looking at opened samba connections

        const QString path = getUrl();
    #else
        const QString path = QDir(getFsPath()).canonicalPath();
    #endif

    const bool result = isLocalPathMounted(path);
    if (!result)
    {
        NX_WARNING(
            this,
            "[initOrUpdate] IsMounted check failed for '%1', local path: '%2'",
            nx::utils::url::hidePassword(getUrl()), path);
    }
    else
    {
        NX_DEBUG(
            this,
            "[initOrUpdate] IsMounted check succeded for '%1', local path: '%2'",
            nx::utils::url::hidePassword(getUrl()), path);
    }

    return result ? Qn::StorageInit_Ok : Qn::StorageInit_WrongPath;
}

QString QnFileStorageResource::getFsPath() const
{
    const QString resourcePath = getPath();
    QnMutexLocker lk(&m_mutex);
    return m_localPath.isEmpty() ? resourcePath : m_localPath;
}

bool QnFileStorageResource::isLocalPathMounted(const QString& path) const
{
    using namespace nx::vms::server::fs::media_paths;
    auto pathConfig = FilterConfig::createDefault(
        m_serverModule->platform(), /*includeNonHdd*/ true, &m_serverModule->settings());

    static const auto normalize =
        [](const QString& s)
        {
            auto result = s;
            result.replace('\\', '/');
            return result;
        };

    const auto mediaPaths = getMediaPaths(pathConfig);
    return std::any_of(
        mediaPaths.cbegin(), mediaPaths.cend(),
        [path = normalize(path)](const auto& p) { return normalize(p).startsWith(path); });
}

Qn::StorageInitResult QnFileStorageResource::testWrite() const
{
    return testWriteCapInternal() ? Qn::StorageInit_Ok : Qn::StorageInit_WrongPath;
}

Qn::StorageInitResult QnFileStorageResource::initOrUpdate()
{
    m_state = initOrUpdateInternal();
    if (m_state != Qn::StorageInit_Ok)
        return m_state;

    m_cachedTotalSpace = rootTool()->totalSpace(getFsPath());
    setIsSystemFlagIfNeeded();

    return m_state;
}

QString QnFileStorageResource::removeProtocolPrefix(const QString& url)
{
    int prefix = url.indexOf("://");
    return prefix == -1 ? url :QUrl(url).path().mid(1);
}

QnStorageResource* QnFileStorageResource::instance(
    QnMediaServerModule* serverModule, const QString&)
{
    return new QnFileStorageResource(serverModule);
}

qint64 QnFileStorageResource::calcInitialSpaceLimit()
{
    auto local = isLocal();
    qint64 baseSpaceLimit = calcSpaceLimit(
        serverModule(),
        local
            ? nx::vms::server::PlatformMonitor::LocalDiskPartition
            : nx::vms::server::PlatformMonitor::NetworkPartition);

    if (m_cachedTotalSpace < 0)
        return baseSpaceLimit;
    else
    {
        qint64 maxSpaceLimit = local ? kMaxLocalStorageSpaceLimit : kMaxNasStorageSpaceLimit;
        if (baseSpaceLimit > maxSpaceLimit) //< User explicitly set large spaceLimit, let's hope he knows what he's doing.
            return baseSpaceLimit;
        return qMin<int64_t>(maxSpaceLimit, qMax<int64_t>(m_cachedTotalSpace.load() / kMaxSpaceLimitRatio, baseSpaceLimit));
    }

    return baseSpaceLimit;
}

qint64 QnFileStorageResource::calcSpaceLimit(
    QnMediaServerModule* serverModule,
    nx::vms::server::PlatformMonitor::PartitionType ptype)
{
    const qint64 defaultStorageSpaceLimit = serverModule->settings().minStorageSpace();
    const bool isLocal =
        ptype == PlatformMonitor::LocalDiskPartition
        || ptype == PlatformMonitor::RemovableDiskPartition;

    return isLocal ? defaultStorageSpaceLimit : kNasStorageLimit;
}

bool QnFileStorageResource::isLocal()
{
    auto url = getUrl();
    if (url.contains(lit("://")))
        return false;

    auto storageTypeString = getStorageType();
    if (!storageTypeString.isEmpty())
    {
        return storageTypeString == QnLexical::serialized(PlatformMonitor::LocalDiskPartition)
            || storageTypeString == QnLexical::serialized(PlatformMonitor::RemovableDiskPartition);
    }

    auto platformMonitor = static_cast<PlatformMonitor*>(serverModule()->platform()->monitor());
    auto partitions = platformMonitor->totalPartitionSpaceInfo(PlatformMonitor::NetworkPartition);

    for (const auto &partition : partitions)
    {
        if (url.startsWith(partition.path))
            return false;
    }

    return true;
}

bool QnFileStorageResource::isMounted() const
{
    return isValid();
}

float QnFileStorageResource::getAvarageWritingUsage() const
{
    QueueFileWriter* writer = QnWriterPool::instance()->getWriter(getId());
    return writer ? writer->getAvarageUsage() : 0;
}
