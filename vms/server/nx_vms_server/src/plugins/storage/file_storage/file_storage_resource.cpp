
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
    return prefix + NX_TEMP_FOLDER_NAME + QString::number(qHash(url), 16);
}

} // namespace <anonymous>


QIODevice* QnFileStorageResource::open(const QString& fileName, QIODevice::OpenMode openMode)
{
    return open(fileName, openMode, 0);
}

QIODevice* QnFileStorageResource::open(
    const QString& url,
    QIODevice::OpenMode openMode,
    int bufferSize)
{
    if (!m_valid)
        return nullptr;

    QString fileName = removeProtocolPrefix(translateUrlToLocal(url));

    int ioBlockSize = 0;
    int ffmpegBufferSize = 0;

    int ffmpegMaxBufferSize =
        m_serverModule->settings().maxFfmpegBufferSize();

    int systemFlags = 0;
    if (openMode & QIODevice::WriteOnly)
    {
        ioBlockSize = m_serverModule->settings().ioBlockSize();

        ffmpegBufferSize = qMax(
            m_serverModule->settings().ffmpegBufferSize(),
            bufferSize);

#ifdef Q_OS_WIN
        if ((openMode & QIODevice::ReadWrite) == QIODevice::ReadWrite)
            systemFlags = 0;
        else if (m_serverModule->settings().disableDirectIO())
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
    return m_serverModule->rootFileSystem();
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

    if (m_serverModule->rootFileSystem()->isPathExists(path))
        return m_serverModule->rootFileSystem()->totalSpace(path);

    if (!m_serverModule->rootFileSystem()->makeDirectory(path))
        return -1;

    result = m_serverModule->rootFileSystem()->totalSpace(path);
    m_serverModule->rootFileSystem()->removePath(path);

    return result;
}

qint64 QnFileStorageResource::calculateAndSetTotalSpaceWithoutInit()
{
    bool valid = false;
    QString url = getUrl();
    qint64 result;

    NX_VERBOSE(this, "%1 valid = %2, url = %3",
           Q_FUNC_INFO,
           m_valid,
           nx::utils::url::hidePassword(url));

    if (url.isNull() || url.isEmpty())
        return -1;
    {
        QnMutexLocker lock(&m_mutexCheckStorage);
        valid = m_valid;
    }
    if (valid == true)
        return getTotalSpace();
    else if (url.contains("://"))
    {
        {
            QnMutexLocker lock(&m_mutexCheckStorage);
            m_valid = mountTmpDrive(url) == Qn::StorageInit_Ok;
        }
        return getTotalSpace();
    }
    // local storage
    result = getDeviceSizeByLocalPossiblyNonExistingPath(url);
    {
        QnMutexLocker locker(&m_writeTestMutex);
        m_cachedTotalSpace = result;
    }

    NX_VERBOSE(this, lit("%1 result = %2")
           .arg(Q_FUNC_INFO)
           .arg(result));

    return result;
}

Qn::StorageInitResult QnFileStorageResource::initOrUpdateInternal()
{
    QString url = getUrl();
    if (m_valid)
        return Qn::StorageInit_Ok;

    Qn::StorageInitResult result = Qn::StorageInit_CreateFailed;


    if (url.isEmpty())
    {
        NX_VERBOSE(this, "[initOrUpdate] storage url is empty");
        return Qn::StorageInit_WrongPath;
    }

    if (url.contains("://"))
    {
        result = mountTmpDrive(url);
    }
    else
    {
        if (rootTool()->isPathExists(url))
        {
            result = Qn::StorageInit_Ok;
        }
        else if (rootTool()->makeDirectory(url))
        {
            result = Qn::StorageInit_Ok;
        }
        else
        {
            NX_VERBOSE(this, "[initOrUpdate] storage dir doesn't exist or mkdir failed");
            result = Qn::StorageInit_WrongPath;
        }
    }

    QString sysPath = sysDrivePath(m_serverModule);
    if (!sysPath.isNull())
        m_isSystem = getDevicePath(m_serverModule, url).startsWith(sysPath);
    else
        m_isSystem = false;

    m_valid = result == Qn::StorageInit_Ok;

    return result;
}

bool QnFileStorageResource::isSystem() const
{
    QnMutexLocker lock(&m_mutexCheckStorage);
    return m_isSystem;
}

bool QnFileStorageResource::checkWriteCap() const
{
    if (!m_valid)
        return false;

    QnMutexLocker lock(&m_writeTestMutex);
    if (!m_writeCapCached.is_initialized())
        m_writeCapCached = testWriteCapInternal();
    return *m_writeCapCached;
}

bool QnFileStorageResource::checkDBCap() const
{
    if (!m_valid)
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
    QList<QnPlatformMonitor::PartitionSpace> partitions =
        m_serverModule->platform()->monitor()->QnPlatformMonitor::totalPartitionSpaceInfo(
            QnPlatformMonitor::NetworkPartition );

    bool dbReady = true;
    for(const QnPlatformMonitor::PartitionSpace& partition: partitions)
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
    if (!m_valid)
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

    const QString prefix = lit("/tmp/") + NX_TEMP_FOLDER_NAME;
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

Qn::StorageInitResult QnFileStorageResource::mountTmpDrive(const QString& urlString)
{
    QUrl url(urlString);
    if (!url.isValid())
        return Qn::StorageInit_WrongPath;

    QString localPath = genLocalPath(getUrl());
    setLocalPathSafe(localPath);

    return rootTool()->remount(url, localPath);
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

Qn::StorageInitResult QnFileStorageResource::updatePermissions(const QString& url) const
{
    if (!url.startsWith("smb://"))
        return Qn::StorageInit_Ok;

    NX_VERBOSE(this, lit("%1 Mounting remote drive %2").arg(Q_FUNC_INFO).arg(nx::utils::url::hidePassword(getUrl())));

    QUrl storageUrl(url);
    NETRESOURCE netRes;
    memset(&netRes, 0, sizeof(netRes));
    netRes.dwType = RESOURCETYPE_DISK;
    QString path = lit("\\\\") + storageUrl.host() + lit("\\") + storageUrl.path().mid((1));
    netRes.lpRemoteName = (LPWSTR) path.constData();

    QString initialUserName = storageUrl.userName().isEmpty() ? "guest" : storageUrl.userName();
    std::array<QString, 2> userNamesToTest = {
        initialUserName,
        lit("WORKGROUP\\") + initialUserName,
    };

    const auto password = storageUrl.password();
    bool wrongAuth = false;

    for (const auto& userName : userNamesToTest)
    {
        auto result = updatePermissionsHelper(
            (LPWSTR) userName.constData(), (LPWSTR) password.constData(), &netRes);
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
    QUrl storageUrl(url);
    if (!storageUrl.isValid())
        return Qn::StorageInit_WrongPath;

    setLocalPathSafe(
        lit("\\\\") +
        storageUrl.host() +
        storageUrl.path().replace(lit("/"), lit("\\")));

    return updatePermissions(url);
}
#endif

void QnFileStorageResource::setUrl(const QString& url)
{
    QnMutexLocker lock(&m_mutexCheckStorage);
    QnStorageResource::setUrl(url);
    m_valid = false;
}

QnFileStorageResource::QnFileStorageResource(QnMediaServerModule* serverModule):
    base_type(serverModule->commonModule()),
    m_valid(false),
    m_capabilities(0),
    m_cachedTotalSpace(QnStorageResource::kUnknownSize),
    m_isSystem(false),
    m_serverModule(serverModule)
{
    m_capabilities |= QnAbstractStorageResource::cap::RemoveFile;
    m_capabilities |= QnAbstractStorageResource::cap::ListFile;
    m_capabilities |= QnAbstractStorageResource::cap::ReadFile;
}

QnFileStorageResource::~QnFileStorageResource()
{
    QnMutexLocker lk(&m_mutex);
#ifndef _WIN32
    if (!m_localPath.isEmpty())
    {
#if __linux__
        auto result = rootTool()->unmount(m_localPath);
        NX_VERBOSE(
            this,
            lm("[mount] unmounting folder %1 while destructing object result: %2")
                .args(m_localPath, nx::SystemCommands::unmountCodeToString(result)));
#elif __APPLE__
        unmount(m_localPath.toLatin1().constData(), 0);
#endif
        rmdir(m_localPath.toLatin1().constData());
    }
#endif
}

bool QnFileStorageResource::removeFile(const QString& url)
{
    if (!m_valid)
        return false;

    m_serverModule->fileDeletor()->deleteFile(removeProtocolPrefix(translateUrlToLocal(url)), getId());
    return true;
}

bool QnFileStorageResource::renameFile(const QString& oldName, const QString& newName)
{
    if (!m_valid)
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
    if (!m_valid)
        return false;

    const auto path = removeProtocolPrefix(translateUrlToLocal(url));
    if (rootTool()->removePath(path))
        return true;

    NX_ERROR(this, lm("removeDir failed for %1").args(path));
    return false;
}

bool QnFileStorageResource::isDirExists(const QString& url)
{
    if (!m_valid)
        return false;

    QString path = removeProtocolPrefix(translateUrlToLocal(url));
    return rootTool()->isPathExists(path);
}

bool QnFileStorageResource::isFileExists(const QString& url)
{
    if (!m_valid)
        return false;

    QString path = removeProtocolPrefix(translateUrlToLocal(url));
    return rootTool()->isPathExists(path);
}

qint64 QnFileStorageResource::getFreeSpace()
{
    QString localPathCopy = getLocalPathSafe();

    if (!m_valid)
        return QnStorageResource::kUnknownSize;

    QString path = localPathCopy.isEmpty() ?  getPath() : localPathCopy;
    return rootTool()->freeSpace(path);
}

qint64 QnFileStorageResource::getTotalSpace() const
{
    if (!m_valid)
        return QnStorageResource::kUnknownSize;

    QString path;
    {
        QnMutexLocker lock(&m_mutex);
        path = m_localPath.isEmpty() ? getPath() : m_localPath;
    }

    QnMutexLocker locker(&m_writeTestMutex);
    if (m_cachedTotalSpace <= 0)
        m_cachedTotalSpace = rootTool()->totalSpace(path);

    return m_cachedTotalSpace;
}

QnAbstractStorageResource::FileInfoList QnFileStorageResource::getFileList(const QString& dirName)
{
    if (!m_valid)
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
    if (!m_valid)
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

Qn::StorageInitResult QnFileStorageResource::initOrUpdate()
{
    NX_VERBOSE(this, "[initOrUpdate] for storage %1 begin", nx::utils::url::hidePassword(getUrl()));

    if (!isMounted())
    {
        NX_VERBOSE(this, "[initOrUpdate] storage %1 is not mounted", nx::utils::url::hidePassword(getUrl()));
        return Qn::StorageInitResult::StorageInit_CreateFailed;
    }

    Qn::StorageInitResult result;
    {
        QnMutexLocker lock(&m_mutexCheckStorage);
        bool oldValid = m_valid;
        result = initOrUpdateInternal();
        if (result != Qn::StorageInit_Ok)
            return result;

        if (!(oldValid == false && m_valid == true) && !isStorageDirMounted())
        {
            NX_VERBOSE(this, lit("[initOrUpdate] storage dir is not mounted. oldValid: %1 valid: %2")
                    .arg(oldValid)
                    .arg(m_valid));
            m_valid = false;
            return Qn::StorageInit_WrongPath;
        }
    }

    QnMutexLocker lock(&m_writeTestMutex);
    m_writeCapCached = testWriteCapInternal(); // update cached value periodically
    // write check fail is a cause to set dirty to true, thus enabling
    // remount attempt in initOrUpdate()
    if (!m_writeCapCached.get())
    {
        NX_ERROR(this, "[initOrUpdate, WriteTest] write test failed for %1",
            nx::utils::url::hidePassword(getUrl()));
        m_valid = false;
        return Qn::StorageInit_WrongPath;
    }
    QString localPath = getLocalPathSafe();
    QString path = localPath.isEmpty() ? getPath() : localPath;
    m_cachedTotalSpace = rootTool()->totalSpace(path); // update cached value periodically
    NX_VERBOSE(
        this,
        "initOrUpdate successfully completed for %1", nx::utils::url::hidePassword(getUrl()));

    return Qn::StorageInit_Ok;
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
        local ? QnPlatformMonitor::LocalDiskPartition : QnPlatformMonitor::NetworkPartition);

    if (m_cachedTotalSpace < 0)
        return baseSpaceLimit;
    else
    {
        qint64 maxSpaceLimit = local ? kMaxLocalStorageSpaceLimit : kMaxNasStorageSpaceLimit;
        if (baseSpaceLimit > maxSpaceLimit) //< User explicitly set large spaceLimit, let's hope he knows what he's doing.
            return baseSpaceLimit;
        return qMin(maxSpaceLimit, qMax(m_cachedTotalSpace / kMaxSpaceLimitRatio, baseSpaceLimit));
    }

    return baseSpaceLimit;
}

qint64 QnFileStorageResource::calcSpaceLimit(QnPlatformMonitor::PartitionType ptype) const
{
    const qint64 defaultStorageSpaceLimit = m_serverModule->settings().minStorageSpace();
    const bool isLocal =
        ptype == QnPlatformMonitor::LocalDiskPartition
        || ptype == QnPlatformMonitor::RemovableDiskPartition;

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
        return storageTypeString == QnLexical::serialized(QnPlatformMonitor::LocalDiskPartition)
            || storageTypeString == QnLexical::serialized(QnPlatformMonitor::RemovableDiskPartition);
    }

    auto platformMonitor = static_cast<QnPlatformMonitor*>(m_serverModule->platform()->monitor());
    auto partitions = platformMonitor->totalPartitionSpaceInfo(QnPlatformMonitor::NetworkPartition);

    for (const auto &partition : partitions)
    {
        if (url.startsWith(partition.path))
            return false;
    }

    return true;
}

void QnFileStorageResource::setMounted(bool value)
{
    QnMutexLocker lock(&m_mutex);
    m_isMounted = value;
}

bool QnFileStorageResource::isMounted() const
{
    QnMutexLocker lock(&m_mutex);
    return m_isMounted;
}

float QnFileStorageResource::getAvarageWritingUsage() const
{
    QueueFileWriter* writer = QnWriterPool::instance()->getWriter(getId());
    return writer ? writer->getAvarageUsage() : 0;
}

#ifdef _WIN32
bool QnFileStorageResource::isStorageDirMounted() const
{
    return true;    //common check is enough on mswin
}

#else

//!Reads mount points (local dirs) from /etc/fstab or /etc/mtab
static bool readTabFile( const QString& filePath, QStringList* const mountPoints )
{
    FILE* tabFile = fopen( filePath.toLocal8Bit().constData(), "r" );
    if( !tabFile )
        return false;

    char* line = nullptr;
    size_t length = 0;
    while( !feof(tabFile) )
    {
        if( getline( &line, &length, tabFile ) == -1 )
            break;

        if( length == 0 || line[0] == '#' )
            continue;

        //TODO #ak regex is actually not needed here, removing it will remove memcpy by QString also
        const QStringList& fields = QString::fromLatin1(line, length).split( QRegExp(QLatin1String("[\\s\\t]+")), QString::SkipEmptyParts );
        if( fields.size() >= 2 )
            mountPoints->push_back( fields[1] );
    }

    free( line );
    fclose( tabFile );

    return true;
}

namespace {
bool findPathInTabFile(const QString& path, const QString& tabFilePath, QString* outMountPoint, bool exact)
{
    QStringList mountPoints;

    if(!readTabFile(tabFilePath, &mountPoints))
    {
        NX_WARNING(typeid(QnFileStorageResource), lit("Could not read %1 file while checking storage %2 availability")
                .arg(tabFilePath)
                .arg(path));
        return false;
    }

    const QString notResolvedStoragePath = closeDirPath(path);
    const QString& storagePath = QDir(notResolvedStoragePath).absolutePath();

    for(const QString& mountPoint: mountPoints )
    {
        const QString& mountPointCanonical = QDir(closeDirPath(mountPoint)).canonicalPath();
        if((!exact && storagePath.startsWith(mountPointCanonical)) || (exact && storagePath == mountPointCanonical))
        {
            *outMountPoint = mountPointCanonical;
            return true;
        }
    }

    return false;
}
} // namespace <anonymous>

bool QnFileStorageResource::isStorageDirMounted() const
{
    QString mountPoint;
    const auto localPath = getLocalPathSafe();
    NX_VERBOSE(this, lit("[initOrUpdate, isStorageDirMounted] local path: %1, getPath(): %2")
            .arg(localPath)
            .arg(getPath()));

    if (!localPath.isEmpty())
        return findPathInTabFile(localPath, lit("/proc/mounts"), &mountPoint, true);
    else if (findPathInTabFile(getPath(), lit("/etc/fstab"), &mountPoint, false))
        return findPathInTabFile(mountPoint, lit("/etc/mtab"), &mountPoint, true);

    return findPathInTabFile(getPath(), lit("/etc/mtab"), &mountPoint, false);
}
#endif    // _WIN32

