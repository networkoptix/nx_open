
#include <iostream>

#include <QtCore/QDir>

#include "file_storage_resource.h"
#include "utils/common/log.h"
#include "utils/common/util.h"
#include "utils/common/buffered_file.h"
#include "recorder/file_deletor.h"
#include "utils/fs/file.h"
#include <common/common_module.h>

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

QIODevice* QnFileStorageResource::open(const QString& url, QIODevice::OpenMode openMode)
{
    if (!isValid())
        return nullptr;

    QString fileName = removeProtocolPrefix(translateUrlToLocal(url));

    int ioBlockSize = 0;
    int ffmpegBufferSize = 0;

    int systemFlags = 0;
    if (openMode & QIODevice::WriteOnly)
    {
        ioBlockSize = MSSettings::roSettings()->value(
            nx_ms_conf::IO_BLOCK_SIZE,
            nx_ms_conf::DEFAULT_IO_BLOCK_SIZE
        ).toInt();

        ffmpegBufferSize = MSSettings::roSettings()->value(
            nx_ms_conf::FFMPEG_BUFFER_SIZE,
            nx_ms_conf::DEFAULT_FFMPEG_BUFFER_SIZE
        ).toInt();;

#ifdef Q_OS_WIN
        if (MSSettings::roSettings()->value(nx_ms_conf::DISABLE_DIRECT_IO).toInt() != 1)
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

    std::unique_ptr<QBufferedFile> rez(
        new QBufferedFile(
            std::shared_ptr<IQnFile>(new QnFile(fileName)),
            ioBlockSize,
            ffmpegBufferSize,
            getId()
        )
    );
    rez->setSystemFlags(systemFlags);
    if (!rez->open(openMode))
        return 0;
    return rez.release();
}

bool QnFileStorageResource::isValid() const
{
    QnMutexLocker lk(&m_mutex);
    return m_valid;
}

void QnFileStorageResource::setLocalPathSafe(const QString &path) const
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

bool QnFileStorageResource::initOrUpdate() const
{
    QnMutexLocker lock(&m_mutexPermission);

    if (getUrl().isEmpty())
        return false;

    bool valid;

    {
        QnMutexLocker lk(&m_mutex);
        valid = m_valid;
    }

    if (!valid)
    {
        if (getUrl().contains("://"))
            valid = mountTmpDrive() == 0; // true if no error code
        else
        {
            valid = true;
            QDir storageDir(getUrl());
            if (!storageDir.exists())
                valid = storageDir.mkpath(getUrl());
        }
    }

    {
        QnMutexLocker lk(&m_mutex);
        m_valid = valid;
    }

    return valid;
}


bool QnFileStorageResource::checkWriteCap() const
{
    if (!isValid())
        return false;

    if (hasFlags(Qn::deprecated))
        return false;

    QnMutexLocker lock(&m_writeTestMutex);
    if (!m_writeCapCached.is_initialized())
        m_writeCapCached = testWriteCapInternal();
    return *m_writeCapCached;

    /*
    QString localDirPath = m_localPath.isEmpty() ? getPath() : m_localPath;
    QDir dir(localDirPath);

    bool needRemoveDir = false;
    if (!dir.exists())  {
        if (!dir.mkpath(localDirPath))
            return false;
        needRemoveDir = true;
    }

    QFile file(closeDirPath(localDirPath) + QString("tmp") + QString::number((unsigned) ((rand() << 16) + rand())));
    bool result = file.open(QFile::WriteOnly);
    if (result) {
        file.close();
        file.remove();
    }

    if (needRemoveDir)
        dir.remove(localDirPath);

    return result;
    */
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
    
    // Same for mounted by hand remote storages (NAS)
    QList<QnPlatformMonitor::PartitionSpace> partitions =
        qnPlatform->monitor()->QnPlatformMonitor::totalPartitionSpaceInfo(
            QnPlatformMonitor::NetworkPartition );

    for(const QnPlatformMonitor::PartitionSpace& partition: partitions)
    {
        if(getPath().startsWith(partition.path))
            return false;
    }
    return true;
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

        if (rmdir(entry.absoluteFilePath().toLatin1().constData()) != 0)
        {
            NX_LOG(
                lit("QnFileStorageResource::removeOldDirs: remove %1 failed").arg(entry.absoluteFilePath()),
                cl_logDEBUG1
            );
        }
    }
#endif
}

#ifndef _WIN32

int QnFileStorageResource::mountTmpDrive() const
{
    QUrl url(getUrl());
    if (!url.isValid())
        return -1;

    QString uncString = url.host() + url.path();
    uncString.replace(lit("/"), lit("\\"));

    QString cifsOptionsString =
        lit("sec=ntlm,username=%1,password=%2,unc=\\\\%3")
            .arg(url.userName())
            .arg(aux::passwordFromUrl(url))
            .arg(uncString);

    QString srcString = lit("//") + url.host() + url.path();
    QString localPathCopy = aux::genLocalPath(getUrl());

    setLocalPathSafe(localPathCopy);

    umount(localPathCopy.toLatin1().constData());
    rmdir(localPathCopy.toLatin1().constData());

    int retCode = mkdir(
        localPathCopy.toLatin1().constData(),
        S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH
    );

#if __linux__
    retCode = mount(
        srcString.toLatin1().constData(),
        localPathCopy.toLatin1().constData(),
        "cifs",
        MS_NODEV | MS_NOEXEC | MS_NOSUID,
        cifsOptionsString.toLatin1().constData()
    );
#elif __APPLE__
#error "TODO BSD-style mount call"
#endif

    if (retCode == -1)
    {
        qWarning()
            << "Mount SMB resource " << srcString
            << " to local path " << localPathCopy << " failed"
            << " retCode: " << retCode << ", errno!: " << errno;
        return -1;
    }

    return 0;
}
#else
bool QnFileStorageResource::updatePermissions() const
{
    if (getUrl().startsWith("smb://"))
    {
        QString userName = QUrl(getUrl()).userName().isEmpty() ?
                           "guest" :
                            QUrl(getUrl()).userName();

        NETRESOURCE netRes;
        memset(&netRes, 0, sizeof(netRes));
        netRes.dwType = RESOURCETYPE_DISK;

        QUrl storageUrl(getUrl());
        QString path = lit("\\\\") + storageUrl.host() +
                       lit("\\") + storageUrl.path().mid((1));

        netRes.lpRemoteName = (LPWSTR) path.constData();

        LPWSTR password = (LPWSTR) storageUrl.password().constData();
        LPWSTR user = (LPWSTR) userName.constData();

        DWORD errCode = WNetUseConnection(
            0,   // window handler, not used
            &netRes,
            password,
            user,
            0,   // connection flags, should work with just 0 though
            0,
            0,   // additional connection info buffer size
            NULL // additional connection info buffer
        );

        if (errCode == ERROR_SESSION_CREDENTIAL_CONFLICT)
        {   // That means that user has alreay used this network resource
            // with different credentials set.
            // If so we can attempt to just use this resource as local one.
            return true;
        }

        if (errCode != NO_ERROR)
            return false;
    }
    return true;
}

int QnFileStorageResource::mountTmpDrive() const
{
    QUrl storageUrl(getUrl());
    if (!storageUrl.isValid())
        return -1;

    QString path =
        lit("\\\\") +
        storageUrl.host() +
        storageUrl.path().replace(lit("/"), lit("\\"));

    if (!updatePermissions())
        return -1;

    setLocalPathSafe(path);

    return 0;
}
#endif

void QnFileStorageResource::setUrl(const QString& url)
{
    QnStorageResource::setUrl(url);
    QnMutexLocker lk(&m_mutex);
    m_valid = false;
}

QnFileStorageResource::QnFileStorageResource():
    m_valid(false),
    m_capabilities(0),
    m_cachedTotalSpace(QnStorageResource::kSizeDetectionOmitted)
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
    if (!isValid())
        return false;

    qnFileDeletor->deleteFile(removeProtocolPrefix(translateUrlToLocal(url)));
    return true;
}

bool QnFileStorageResource::renameFile(const QString& oldName, const QString& newName)
{
    if (!isValid())
        return false;

    return QFile::rename(translateUrlToLocal(oldName), translateUrlToLocal(newName));
}


bool QnFileStorageResource::removeDir(const QString& url)
{
    if (!isValid())
        return false;

    QDir dir;
    return dir.rmdir(removeProtocolPrefix(translateUrlToLocal(url)));
}

bool QnFileStorageResource::isDirExists(const QString& url)
{
    if (!isValid())
        return false;

    QDir d(translateUrlToLocal(url));
    return d.exists(removeProtocolPrefix(translateUrlToLocal(url)));
}

bool QnFileStorageResource::isFileExists(const QString& url)
{
    if (!isValid())
        return false;

    return QFile::exists(removeProtocolPrefix(translateUrlToLocal(url)));
}

qint64 QnFileStorageResource::getFreeSpace()
{
    QString localPathCopy = getLocalPathSafe();

    if (!isValid())
        return QnStorageResource::kUnknownSize;

    return getDiskFreeSpace(localPathCopy.isEmpty() ?  getPath() : localPathCopy);
}

qint64 QnFileStorageResource::getTotalSpace()
{
    if (!isValid())
        return QnStorageResource::kUnknownSize;

    QString localPathCopy = getLocalPathSafe();

    QnMutexLocker locker (&m_writeTestMutex);
    if (m_cachedTotalSpace <= 0)
        m_cachedTotalSpace = getDiskTotalSpace(
            localPathCopy.isEmpty() ? getPath() : localPathCopy
        );
    return m_cachedTotalSpace;
}

QnAbstractStorageResource::FileInfoList QnFileStorageResource::getFileList(const QString& dirName)
{
    if (!isValid())
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
    if (!isValid())
        return -1;

    return QnFile::getFileSize(translateUrlToLocal(url));
}

bool QnFileStorageResource::testWriteCapInternal() const
{
    QString fileName(lit("%1%2.tmp"));
    QString localGuid = qnCommon->moduleGUID().toString();
    localGuid = localGuid.mid(1, localGuid.length() - 2);
    fileName = fileName.arg(closeDirPath(translateUrlToLocal(getPath()))).arg(localGuid);
    QFile file(fileName);
    return file.open(QIODevice::WriteOnly);
}

bool QnFileStorageResource::isAvailable() const
{
    QString localPathCopy;
    {
        QnMutexLocker lk(&m_mutex);
        localPathCopy = m_localPath;
    }

    if (!initOrUpdate())
        return false;

    if (!isStorageDirMounted())
    {
        QnMutexLocker lk(&m_mutex);
        m_valid = false;
        return false;
    }

    QnMutexLocker lock(&m_writeTestMutex);
    m_writeCapCached = testWriteCapInternal(); // update cached value periodically
    // write check fail is a cause to set dirty to true, thus enabling
    // remount attempt in initOrUpdate()
    if (!m_writeCapCached)
    {
        QnMutexLocker lk(&m_mutex);
        m_valid = false;
    }
    m_cachedTotalSpace = getDiskTotalSpace(
        localPathCopy.isEmpty() ? getPath() : localPathCopy
    ); // update cached value periodically
    return *m_writeCapCached;

    /*
    QString tmpDir = closeDirPath(translateUrlToLocal(getPath())) + QString("tmp") + QString::number(rand());
    QDir dir(tmpDir);
    if (dir.exists()) {
        dir.remove(tmpDir);
        return true;
    }
    else {
        if (dir.mkpath(tmpDir))
        {
            dir.rmdir(tmpDir);
            return true;
        }
        else {
#ifdef WIN32
            if (::GetLastError() == ERROR_DISK_FULL)
                return true;
#else
            if (errno == ENOSPC)
                return true;
#endif
            return false;
        }
    }
    return false;
    */
}

QString QnFileStorageResource::removeProtocolPrefix(const QString& url)
{
    int prefix = url.indexOf("://");
    return prefix == -1 ? url :QUrl(url).path().mid(1);
}

QnStorageResource* QnFileStorageResource::instance(const QString&)
{
    QnStorageResource* storage = new QnFileStorageResource();
    storage->setSpaceLimit(
        MSSettings::roSettings()->value(
            nx_ms_conf::MIN_STORAGE_SPACE,
            nx_ms_conf::DEFAULT_MIN_STORAGE_SPACE
        ).toLongLong()
    );
    return storage;
}

qint64 QnFileStorageResource::calcSpaceLimit(const QString &url)
{
    const qint64 defaultStorageSpaceLimit = MSSettings::roSettings()->value(
        nx_ms_conf::MIN_STORAGE_SPACE,
        nx_ms_conf::DEFAULT_MIN_STORAGE_SPACE
    ).toLongLong();

    return QnFileStorageResource::isLocal(url) ?  defaultStorageSpaceLimit :
                                                  QnStorageResource::kNasStorageLimit;
}

qint64 QnFileStorageResource::calcSpaceLimit(QnPlatformMonitor::PartitionType ptype)
{
    const qint64 defaultStorageSpaceLimit = MSSettings::roSettings()->value(
        nx_ms_conf::MIN_STORAGE_SPACE,
        nx_ms_conf::DEFAULT_MIN_STORAGE_SPACE
    ).toLongLong();

    return ptype == QnPlatformMonitor::LocalDiskPartition ?
           defaultStorageSpaceLimit :
           QnStorageResource::kNasStorageLimit;
}

bool QnFileStorageResource::isLocal(const QString &url)
{
    if (url.contains(lit("://")))
        return false;

    auto platformMonitor = static_cast<QnPlatformMonitor*>(qnPlatform->monitor());

    QList<QnPlatformMonitor::PartitionSpace> partitions =
        platformMonitor->totalPartitionSpaceInfo(
            QnPlatformMonitor::LocalDiskPartition
        );

    for (const auto &partition : partitions)
        if (url.startsWith(partition.path))
            return true;

    return false;
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


bool QnFileStorageResource::isStorageDirMounted() const
{
    QString localPathCopy = getLocalPathSafe();

    if (!localPathCopy.isEmpty()) // smb
    {
        QUrl url(getUrl());

        QString uncString = url.host() + url.path();
        uncString.replace(lit("/"), lit("\\"));

        QString cifsOptionsString =
            lit("sec=ntlm,username=%1,password=%2,unc=\\\\%3")
                .arg(url.userName())
                .arg(aux::passwordFromUrl(url))
                .arg(uncString);

        QString srcString = lit("//") + url.host() + url.path();

        auto badResultHandler = [this, &localPathCopy]()
        {   // Treat every unexpected test result the same way.
            // Cleanup attempt will be performed.
            // Will try to unmount, remove local mount point directory
            // and set flag meaning that local path recreation
            // + remount is needed
            umount(localPathCopy.toLatin1().constData()); // directory may not exists, but this is Ok
            rmdir(localPathCopy.toLatin1().constData()); // same as above
            return false;
        };

        // Check if local (mounted) storage path exists
        DIR* dir = opendir(localPathCopy.toLatin1().constData());
        if (dir)
            closedir(dir);
        else if (ENOENT == errno)
        {   // Directory does not exist.
            NX_LOG(
                lit("[QnFileStorageResource::isStorageDirMounted] opendir() %1 failed, directory does not exists")
                    .arg(localPathCopy),
                cl_logDEBUG1
            );
            return badResultHandler();
        }
        else
        {   // opendir() failed for some other reason.
            NX_LOG(
                lit("[QnFileStorageResource::isStorageDirMounted] opendir() %1 failed, errno = %2")
                    .arg(localPathCopy)
                    .arg(errno),
                cl_logDEBUG1
            );
            return badResultHandler();
        }

#if __linux__
        int retCode = mount(
            srcString.toLatin1().constData(),
            localPathCopy.toLatin1().constData(),
            "cifs",
            MS_NOSUID | MS_NODEV | MS_NOEXEC,
            cifsOptionsString.toLatin1().constData()
        );
#elif __APPLE__
#error "TODO BSD-style mount call"
#endif

        int err = errno;

        if ((retCode == -1 && err != EBUSY) || retCode == 0)
        {   // Bad. Mount succeeded OR it failed but for another reason than
            // local path is already mounted.
            NX_LOG(
                lit("[QnFileStorageResource::isStorageDirMounted] mount result = %1 , errno = %2")
                    .arg(retCode)
                    .arg(err),
                cl_logDEBUG1
            );
            return badResultHandler();
        }

        return true;
    }

    // This part is for manually mounted folders

    const QString notResolvedStoragePath = closeDirPath(getPath());
    const QString& storagePath = QDir(notResolvedStoragePath).absolutePath();
    //on unix, checking that storage directory is mounted, if it is to be mounted

    QStringList mountPoints;
    if( !readTabFile( lit("/etc/fstab"), &mountPoints ) )
    {
        NX_LOG( lit("Could not read /etc/fstab file while checking storage %1 availability").arg(storagePath), cl_logWARNING );
        return false;
    }

    //checking if storage dir is to be mounted
    QString storageMountPoint;
    for(const QString& mountPoint: mountPoints )
    {
        const QString& mountPointCanonical = QDir(closeDirPath(mountPoint)).canonicalPath();
        if( storagePath.startsWith(mountPointCanonical) )
            storageMountPoint = mountPointCanonical;
    }

    //checking, if it has been mounted
    if( !storageMountPoint.isEmpty() )
    {
        QStringList mountedDirectories;
        //reading /etc/mtab
        if( !readTabFile( QLatin1String("/etc/mtab"), &mountedDirectories ) )
        {
            NX_LOG( lit("Could not read /etc/mtab file while checking storage %1 availability").arg(storagePath), cl_logWARNING );
            return false;
        }
        if( !mountedDirectories.contains(storageMountPoint) )
        {
            NX_LOG( lit("Storage %1 mount directory has not been mounted yet (mount point %2)").arg(storagePath).arg(storageMountPoint), cl_logWARNING );
            return false;  //storage directory has not been mounted yet
        }
    }

    NX_LOG( lit("Storage %1 is mounted (mount point %2)").arg(storagePath).arg(storageMountPoint), cl_logDEBUG2 );
    return true;
}
#endif    // _WIN32

