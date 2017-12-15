
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
#include <nx/mediaserver/root_tool.h>

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
#   include <sys/mount.h>
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
const QString& getDevicePath(const QString& path)
{
    return path;
}

const QString& sysDrivePath()
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

const QString getDevicePath(const QString& path)
{
    QString command = lit("df '") + path + lit("'");
    FILE* pipe;
    char buf[BUFSIZ];

    if ((pipe = popen(command.toLatin1().constData(), "r")) == NULL)
    {
        NX_LOG(lit("%1 'df' call failed").arg(Q_FUNC_INFO), cl_logWARNING);
        return QString();
    }

    if (fgets(buf, BUFSIZ, pipe) == NULL) // header line
    {
        pclose(pipe);
        return QString();
    }

    if (fgets(buf, BUFSIZ, pipe) == NULL) // data
    {
        pclose(pipe);
        return QString();
    }

    auto dataString = QString::fromUtf8(buf);
    QString deviceString = dataString.section(QRegularExpression("\\s+"), 0, 0);

    pclose(pipe);

    return deviceString;
}

const QString& sysDrivePath()
{
    static QString devicePath = getDevicePath(lit("/"));
    return devicePath;
}

#else // Unsupported OS so far

const QString& getDevicePath(const QString& path)
{
    return path;
}

const QString& sysDrivePath()
{
    return QString();
}

#endif

} // namespace <anonymous>


namespace aux
{
    QString genLocalPath(const QString &url, const QString &prefix = "/tmp/")
    {
        return prefix + NX_TEMP_FOLDER_NAME + QString::number(qHash(url), 16);
    }

    QString passwordFromUrl(const QUrl &url)
    {   // On some linux distribution (nx1) mount chokes on
        // empty password. So let's give it a non-empty one.
        QString password = url.password();
        return password.isEmpty() ? "123" : password;
    }
}

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
        qnServerModule->roSettings()->value(
            nx_ms_conf::MAX_FFMPEG_BUFFER_SIZE,
            nx_ms_conf::DEFAULT_MAX_FFMPEG_BUFFER_SIZE).toInt();

    int systemFlags = 0;
    if (openMode & QIODevice::WriteOnly)
    {
        ioBlockSize = qnServerModule->roSettings()->value(
            nx_ms_conf::IO_BLOCK_SIZE,
            nx_ms_conf::DEFAULT_IO_BLOCK_SIZE).toInt();

        ffmpegBufferSize = qMax(
            qnServerModule->roSettings()->value(
                nx_ms_conf::FFMPEG_BUFFER_SIZE,
                nx_ms_conf::DEFAULT_FFMPEG_BUFFER_SIZE).toInt(),
            bufferSize);

#ifdef Q_OS_WIN
        if ((openMode & QIODevice::ReadWrite) == QIODevice::ReadWrite)
            systemFlags = 0;
        else if (qnServerModule->roSettings()->value(nx_ms_conf::DISABLE_DIRECT_IO).toInt() != 1)
            systemFlags = FILE_FLAG_NO_BUFFERING;
#endif
    }

    /*
    if (openMode & QIODevice::WriteOnly)
    {
        QDir dir;
        dir.mkpath(QnFile::absolutePath(fileName));
    }
    */
    if (openMode.testFlag(QIODevice::Unbuffered))
        ioBlockSize = ffmpegBufferSize = 0;

    std::unique_ptr<QBufferedFile> rez(
        new QBufferedFile(
            std::shared_ptr<IQnFile>(new QnFile(fileName)),
                ioBlockSize,
                ffmpegBufferSize,
                ffmpegMaxBufferSize,
                getId()));
    rez->setSystemFlags(systemFlags);
    if (rez->open(openMode))
        return rez.release();

    if (const auto rootTool = qnServerModule->rootTool())
    {
        if (rootTool->touchFile(fileName) && rez->open(openMode))
            return rez.release();
    }

    return nullptr;
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

qint64 getLocalPossiblyNonExistingPathSize(const QString &path)
{
    qint64 result;

    if (QDir(path).exists())
        return getDiskTotalSpace(path);

    if (!QDir().mkpath(path))
        return -1;

    result = getDiskTotalSpace(path);
    QDir(path).removeRecursively();

    return result;
}

qint64 QnFileStorageResource::calculateAndSetTotalSpaceWithoutInit()
{
    bool valid = false;
    QString url = getUrl();
    qint64 result;

    NX_LOG(lit("%1 valid = %2, url = %3")
           .arg(Q_FUNC_INFO)
           .arg(m_valid)
           .arg(url), cl_logDEBUG2);

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
    result = getLocalPossiblyNonExistingPathSize(url);
    {
        QnMutexLocker locker(&m_writeTestMutex);
        m_cachedTotalSpace = result;
    }

    NX_LOG(lit("%1 result = %2")
           .arg(Q_FUNC_INFO)
           .arg(result), cl_logDEBUG2);

    return result;
}

Qn::StorageInitResult QnFileStorageResource::initOrUpdateInternal()
{
    if (m_valid)
        return Qn::StorageInit_Ok;

    Qn::StorageInitResult result = Qn::StorageInit_CreateFailed;
    QString url = getUrl();

    if (url.isEmpty())
    {
        NX_LOG("[initOrUpdate] storage url is empty", cl_logDEBUG2);
        return Qn::StorageInit_WrongPath;
    }

    if (url.contains("://"))
    {
        result = mountTmpDrive(url);
    }
    else
    {
        const auto rootTool = qnServerModule->rootTool();
        QDir storageDir(url);
        if (storageDir.exists() || storageDir.mkpath(url))
        {
            if (rootTool)
                rootTool->changeOwner(url); //< Just in case it was not ours.

            result = Qn::StorageInit_Ok;
        }
        else
        {
            if (rootTool && rootTool->makeDirectory(url))
            {
                result = Qn::StorageInit_Ok;
            }
            else
            {
                NX_LOG("[initOrUpdate] storage dir doesn't exist or mkdir failed", cl_logDEBUG2);
                result = Qn::StorageInit_WrongPath;
            }
        }
    }

    QString sysPath = sysDrivePath();
    if (!sysPath.isNull())
        m_isSystem = getDevicePath(url).startsWith(sysPath);
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
        qnPlatform->monitor()->QnPlatformMonitor::totalPartitionSpaceInfo(
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

void QnFileStorageResource::removeOldDirs()
{
#ifndef _WIN32
    QFileInfoList tmpEntries = QDir("/tmp").entryInfoList(
        QDir::AllDirs | QDir::NoDotAndDotDot
    );

    const QString prefix = lit("/tmp/") + NX_TEMP_FOLDER_NAME;

    for (const QFileInfo &entry : tmpEntries)
    {
        if (entry.absoluteFilePath().indexOf(prefix) == -1)
            continue;

#if __linux__
        int ecode = umount(entry.absoluteFilePath().toLatin1().constData());
        if (ecode != 0)
        {
            if (const auto rootTool = qnServerModule->rootTool())
            {
                if (rootTool->unmount(entry.absoluteFilePath()))
                    ecode = 0;
            }
        }
#elif __APPLE__
        int ecode = unmount(entry.absoluteFilePath().toLatin1().constData(), 0);
#endif
        if (ecode != 0)
        {
            bool safeToRemove = true;

            switch (errno)
            {
            case EBUSY:
            case ENOMEM:
            case EPERM:
                safeToRemove = false;
                break;
            }

            if (!safeToRemove)
            {
                NX_LOG(
                    lit("QnFileStorageResource::removeOldDirs: umount %1 failed").arg(entry.absoluteFilePath()),
                    cl_logDEBUG1
                );
                continue;
            }
        }

        if (rmdir(entry.absoluteFilePath().toLatin1().constData()) == 0)
            continue;

        if (const auto rootTool = qnServerModule->rootTool())
        {
            if (rootTool->changeOwner(entry.absoluteFilePath())
                && rmdir(entry.absoluteFilePath().toLatin1().constData()))
            {
                continue;
            }
        }

        NX_LOG(
            lit("QnFileStorageResource::removeOldDirs: remove %1 failed").arg(entry.absoluteFilePath()),
            cl_logDEBUG1
        );
    }
#endif
}

#ifndef _WIN32

namespace {
QString prepareCommandString(const QUrl& url, const QString& localPath)
{
    QString cifsOptionsString = lit("rsize=8192,wsize=8192");
    if (!url.userName().isEmpty())
        cifsOptionsString +=
            lit(",username=%2,password=%3")
                .arg(url.userName())
                .arg(aux::passwordFromUrl(url));
    else
        cifsOptionsString += lit(",password=");

    QString srcString = lit("//") + url.host() + url.path();
    return lit("mount -t cifs -o %1 %2 %3 2>&1")
                .arg(cifsOptionsString)
                .arg(srcString)
                .arg(localPath);
}

int callMount(const QString& commandString)
{
    FILE* pipe;
    char buf[BUFSIZ];
    int retCode = -1;

    if ((pipe = popen(commandString.toLatin1().constData(), "r")) == NULL)
    {
        NX_LOG(lit("%1 'mount' call failed").arg(Q_FUNC_INFO), cl_logWARNING);
        return -1;
    }

    while (fgets(buf, BUFSIZ, pipe) != NULL)
    {
        QString outputString = QString::fromUtf8(buf);
        if (outputString.contains(lit("mount error")) && outputString.contains(lit("13")))
        {
            retCode = EACCES;
            break;
        }
    }

    int processReturned = pclose(pipe);
    return retCode == -1 ? processReturned : retCode;
}
}

Qn::StorageInitResult QnFileStorageResource::mountTmpDrive(const QString& urlString)
{
    QUrl url(urlString);
    if (!url.isValid())
        return Qn::StorageInit_WrongPath;

    QString localPath = aux::genLocalPath(getUrl());
    setLocalPathSafe(localPath);
    if (const auto rootTool = qnServerModule->rootTool())
    {
        if (rootTool->remount(url, localPath))
            return Qn::StorageInit_Ok;

        NX_WARNING(this, lm("Failed to mount '%1' to '%2' by root tool").args(url, localPath));
    }

    umount(localPath.toLatin1().constData());
    rmdir(localPath.toLatin1().constData());

    int retCode = mkdir(localPath.toLatin1().constData(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (retCode != 0)
    {
        NX_LOG(lit("%1 mkdir %2 failed").arg(Q_FUNC_INFO).arg(localPath), cl_logWARNING);
        return Qn::StorageInit_WrongPath;
    }

#if __linux__
    retCode = callMount(prepareCommandString(url, localPath));
#elif __APPLE__
#error "TODO BSD-style mount call"
#endif

    if (retCode != 0)
    {
        NX_LOG(lit("Mount SMB resource call with options '%1' failed with retCode %2")
                .arg(prepareCommandString(url, localPath))
                .arg(retCode),
               cl_logWARNING);
        return retCode == EACCES ? Qn::StorageInit_WrongAuth : Qn::StorageInit_WrongPath;
    }

    return Qn::StorageInit_Ok;
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
        NX_LOG(lit("%1 Mounting remote drive %2. Result: %3")
               .arg(Q_FUNC_INFO)
               .arg(getUrl())
               .arg(message), cl_logDEBUG1);
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
            NX_LOG(lit("%1 Mounting remote drive %2 error %3.")
                    .arg(Q_FUNC_INFO)
                    .arg(getUrl())
                    .arg(errCode), cl_logWARNING);
            return Qn::StorageInit_WrongPath;
    };

#undef STR

    return Qn::StorageInit_WrongPath;
}

Qn::StorageInitResult QnFileStorageResource::updatePermissions(const QString& url) const
{
    if (!url.startsWith("smb://"))
        return Qn::StorageInit_Ok;

    NX_LOG(lit("%1 Mounting remote drive %2").arg(Q_FUNC_INFO).arg(getUrl()), cl_logDEBUG2);

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

    LPWSTR password = (LPWSTR) storageUrl.password().constData();
    bool wrongAuth = false;

    for (const auto& userName : userNamesToTest)
    {
        auto result = updatePermissionsHelper((LPWSTR) userName.constData(), password, &netRes);
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

QnFileStorageResource::QnFileStorageResource(QnCommonModule* commonModule):
    base_type(commonModule),
    m_valid(false),
    m_capabilities(0),
    m_cachedTotalSpace(QnStorageResource::kUnknownSize),
    m_isSystem(false)
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
        umount(m_localPath.toLatin1().constData());
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

    qnFileDeletor->deleteFile(removeProtocolPrefix(translateUrlToLocal(url)), getId());
    return true;
}

bool QnFileStorageResource::renameFile(const QString& oldName, const QString& newName)
{
    if (!m_valid)
        return false;

    const auto oldPath = translateUrlToLocal(oldName);
    const auto newPath = translateUrlToLocal(newName);
    if (QFile::rename(oldPath, newPath))
        return true;

    if (const auto rootTool = qnServerModule->rootTool())
    {
        if (rootTool->changeOwner(oldPath) && rootTool->touchFile(newPath))
            return QFile::rename(oldPath, newPath);
    }

    return false;
}


bool QnFileStorageResource::removeDir(const QString& url)
{
    if (!m_valid)
        return false;

    const auto path = removeProtocolPrefix(translateUrlToLocal(url));
    QDir dir;
    if (dir.rmdir(path))
        return true;

    if (const auto rootTool = qnServerModule->rootTool())
        return rootTool->changeOwner(path);

    return dir.rmdir(path);
}

bool QnFileStorageResource::isDirExists(const QString& url)
{
    if (!m_valid)
        return false;

    QDir d(translateUrlToLocal(url));
    return d.exists(removeProtocolPrefix(translateUrlToLocal(url)));
}

bool QnFileStorageResource::isFileExists(const QString& url)
{
    if (!m_valid)
        return false;

    return QFile::exists(removeProtocolPrefix(translateUrlToLocal(url)));
}

qint64 QnFileStorageResource::getFreeSpace()
{
    QString localPathCopy = getLocalPathSafe();

    if (!m_valid)
        return QnStorageResource::kUnknownSize;

    return getDiskFreeSpace(localPathCopy.isEmpty() ?  getPath() : localPathCopy);
}

qint64 QnFileStorageResource::getTotalSpace() const
{
    if (!m_valid)
        return QnStorageResource::kUnknownSize;

    QnMutexLocker locker(&m_writeTestMutex);
    if (m_cachedTotalSpace <= 0)
    {
        m_cachedTotalSpace = getDiskTotalSpace(
            m_localPath.isEmpty() ? getPath() : m_localPath);
    }

    return m_cachedTotalSpace;
}

QnAbstractStorageResource::FileInfoList QnFileStorageResource::getFileList(const QString& dirName)
{
    if (!m_valid)
        return QnAbstractStorageResource::FileInfoList();

    QnAbstractStorageResource::FileInfoList ret;

    QDir dir(translateUrlToLocal(dirName));
    if (!dir.exists())
        return ret;

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
                entry.isDir()
            )
        );
    }
    return ret;
}

qint64 QnFileStorageResource::getFileSize(const QString& url) const
{
    if (!m_valid)
        return -1;

    return QnFile::getFileSize(translateUrlToLocal(url));
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

    if (const auto rootTool = qnServerModule->rootTool())
        return rootTool->touchFile(fileName) && QFile::remove(fileName);

    return false;
}

Qn::StorageInitResult QnFileStorageResource::initOrUpdate()
{
    NX_LOG(lit("[initOrUpdate] for storage %1 begin").arg(getUrl()), cl_logDEBUG2);

    Qn::StorageInitResult result;
    {
        QnMutexLocker lock(&m_mutexCheckStorage);
        bool oldValid = m_valid;
        result = initOrUpdateInternal();
        if (result != Qn::StorageInit_Ok)
            return result;

        if (!(oldValid == false && m_valid == true) && !isStorageDirMounted())
        {
            NX_LOG(lit("[initOrUpdate] storage dir is not mounted. oldValid: %1 valid: %2")
                    .arg(oldValid)
                    .arg(m_valid), cl_logDEBUG2);
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
        NX_LOG("[initOrUpdate] write test file failed", cl_logDEBUG2);
        m_valid = false;
        return Qn::StorageInit_WrongPath;
    }
    QString localPath = getLocalPathSafe();
    m_cachedTotalSpace = getDiskTotalSpace(localPath.isEmpty() ? getPath() : localPath); // update cached value periodically
    NX_LOG("QnFileStorageResource::initOrUpdate completed", cl_logDEBUG2);

    return Qn::StorageInit_Ok;
}

QString QnFileStorageResource::removeProtocolPrefix(const QString& url)
{
    int prefix = url.indexOf("://");
    return prefix == -1 ? url :QUrl(url).path().mid(1);
}

QnStorageResource* QnFileStorageResource::instance(QnCommonModule* commonModule, const QString&)
{
    return new QnFileStorageResource(commonModule);
}

qint64 QnFileStorageResource::calcInitialSpaceLimit()
{
    auto local = isLocal();
    qint64 baseSpaceLimit = calcSpaceLimit(
        local ? QnPlatformMonitor::LocalDiskPartition :
        QnPlatformMonitor::NetworkPartition);

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

qint64 QnFileStorageResource::calcSpaceLimit(QnPlatformMonitor::PartitionType ptype)
{
    const qint64 defaultStorageSpaceLimit = qnServerModule->roSettings()->value(
        nx_ms_conf::MIN_STORAGE_SPACE,
        nx_ms_conf::DEFAULT_MIN_STORAGE_SPACE
    ).toLongLong();

    return ptype == QnPlatformMonitor::LocalDiskPartition ?
           defaultStorageSpaceLimit :
           QnStorageResource::kNasStorageLimit;
}

bool QnFileStorageResource::isLocal()
{
    auto url = getUrl();
    if (url.contains(lit("://")))
        return false;

    auto storageTypeString = getStorageType();
    if (!storageTypeString.isEmpty())
    {
       if (storageTypeString == QnLexical::serialized(QnPlatformMonitor::LocalDiskPartition))
          return true;
       return false;
    }

    auto platformMonitor = static_cast<QnPlatformMonitor*>(qnPlatform->monitor());
    auto partitions = platformMonitor->totalPartitionSpaceInfo(QnPlatformMonitor::NetworkPartition);

    for (const auto &partition : partitions)
    {
        if (url.startsWith(partition.path))
            return false;
    }

    return true;
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
        NX_LOG(
            lit("Could not read %1 file while checking storage %2 availability")
                .arg(tabFilePath)
                .arg(path),
            cl_logWARNING);
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

    NX_LOG(lit("[initOrUpdate, isStorageDirMounted] local path: %1, getPath(): %2")
            .arg(m_localPath)
            .arg(getPath()), cl_logDEBUG2);

    if (!m_localPath.isEmpty())
        return findPathInTabFile(m_localPath, lit("/proc/mounts"), &mountPoint, true);
    else if (findPathInTabFile(getPath(), lit("/etc/fstab"), &mountPoint, false))
        return findPathInTabFile(mountPoint, lit("/etc/mtab"), &mountPoint, true);

    return findPathInTabFile(getPath(), lit("/etc/mtab"), &mountPoint, false);
}
#endif    // _WIN32

