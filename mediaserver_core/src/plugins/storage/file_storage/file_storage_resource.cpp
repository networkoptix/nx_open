
#include <iostream>

#include <QtCore/QDir>

#include "file_storage_resource.h"
#include "utils/common/log.h"
#include "utils/common/util.h"
#include "utils/common/buffered_file.h"
#include "recorder/file_deletor.h"
#include "utils/fs/file.h"

#ifndef _WIN32
#   include <platform/monitoring/global_monitor.h>
#   include <platform/platform_abstraction.h>
#endif

#ifdef Q_OS_WIN
#include "windows.h"
#endif
#include <media_server/settings.h>
#include <recorder/storage_manager.h>

#ifdef __linux__
#   include <sys/mount.h>
#   include <sys/types.h>
#   include <sys/stat.h>
#   include <fcntl.h>
#   include <unistd.h>
#   include <errno.h>
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



QIODevice* QnFileStorageResource::open(const QString& url, QIODevice::OpenMode openMode)
{
    if (!initOrUpdate())
        return nullptr;

    QString fileName = removeProtocolPrefix(translateUrlToLocal(url));

    int ioBlockSize = 0;
    int ffmpegBufferSize = 0;

    int systemFlags = 0;
    if (openMode & QIODevice::WriteOnly) {
        ioBlockSize = MSSettings::roSettings()->value( nx_ms_conf::IO_BLOCK_SIZE, nx_ms_conf::DEFAULT_IO_BLOCK_SIZE ).toInt();
        ffmpegBufferSize = MSSettings::roSettings()->value( nx_ms_conf::FFMPEG_BUFFER_SIZE, nx_ms_conf::DEFAULT_FFMPEG_BUFFER_SIZE ).toInt();;
#ifdef Q_OS_WIN
        if (MSSettings::roSettings()->value(nx_ms_conf::DISABLE_DIRECT_IO).toInt() != 1)
            systemFlags = FILE_FLAG_NO_BUFFERING;
#endif
    }
    
    if (openMode & QIODevice::WriteOnly) 
    {
        QDir dir;
        dir.mkpath(QnFile::absolutePath(fileName));
    }

    std::unique_ptr<QBufferedFile> rez(
        new QBufferedFile(
            std::shared_ptr<IQnFile>(new QnFile(fileName)), 
            ioBlockSize, 
            ffmpegBufferSize
        ) 
    );
    rez->setSystemFlags(systemFlags);
    if (!rez->open(openMode))
        return 0;
    return rez.release();
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
    QMutexLocker lock(&m_mutexPermission);
    
    if (getUrl().isEmpty())
        return false;

    if (m_dirty)
    {
        m_dirty = false;
        if (getUrl().contains("://"))
            m_valid = mountTmpDrive() == 0; // true if no error code
        else
            m_valid = true;
    }
    return m_valid;
}


bool QnFileStorageResource::checkWriteCap() const
{
    if (!initOrUpdate())
        return false;

    if( !isStorageDirMounted() )
        return false;
    
    if (hasFlags(Qn::deprecated))
        return false;
    
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
}

bool QnFileStorageResource::checkDBCap() const
{
    if (!initOrUpdate() || !m_localPath.isEmpty())
        return false;
#ifdef _WIN32
    return true;
#else    
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
    if (!initOrUpdate())
        return 0;

    if (checkDBCap())
        m_capabilities |= QnAbstractStorageResource::cap::DBReady;
    return m_capabilities | (checkWriteCap() ? QnAbstractStorageResource::cap::WriteFile : 0);
}

QString QnFileStorageResource::translateUrlToLocal(const QString &url) const
{
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
        QStringList() << (lit("*") + NX_TEMP_FOLDER_NAME + lit("*")),
        QDir::AllDirs | QDir::NoDotAndDotDot
    );

    for (const QFileInfo &entry : tmpEntries)
    {
        int ecode = umount(entry.absoluteFilePath().toLatin1().constData());
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

    QString cifsOptionsString =
        lit("username=%1,password=%2")
            .arg(url.userName())
            .arg(url.password());

    QString srcString = lit("//") + url.host() + url.path();

    auto randomString = []
    {
        std::stringstream randomStringStream;
        randomStringStream << std::hex << std::rand() << std::rand();

        return randomStringStream.str();
    };

    m_localPath = "/tmp/" + NX_TEMP_FOLDER_NAME + QString::fromStdString(randomString());
    int retCode = rmdir(m_localPath.toLatin1().constData());

    retCode = mkdir(
        m_localPath.toLatin1().constData(),
        S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH
    );

    retCode = mount(
        srcString.toLatin1().constData(),
        m_localPath.toLatin1().constData(),
        "cifs",
        MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_SILENT,
        cifsOptionsString.toLatin1().constData()
    );

    if (retCode == -1)
    {
        qWarning()
            << "Mount SMB resource " << srcString
            << " to local path " << m_localPath << " failed";
        return -1;
    }

    return 0;
}
#else
bool QnFileStorageResource::updatePermissions() const
{
    if (getUrl().startsWith("smb://") && !QUrl(getUrl()).userName().isEmpty())
    {
        NETRESOURCE netRes;
        memset(&netRes, 0, sizeof(netRes));
        netRes.dwType = RESOURCETYPE_DISK;
        QUrl storageUrl(getUrl());
        QString path = lit("\\\\") + storageUrl.host() + lit("\\") + storageUrl.path().mid((1));
        netRes.lpRemoteName = (LPWSTR) path.constData();
        LPWSTR password = (LPWSTR) storageUrl.password().constData();
        LPWSTR user = (LPWSTR) storageUrl.userName().constData();
        if (WNetUseConnection(0, &netRes, password, user, 0, 0, 0, 0) != NO_ERROR)
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

    m_localPath = path;
    return 0;
}
#endif

void QnFileStorageResource::setUrl(const QString& url)
{
    QMutexLocker lock(&m_mutex);
    QnStorageResource::setUrl(url);
    m_dirty = true;
}

QnFileStorageResource::QnFileStorageResource(QnStorageManager *storageManager):
    m_storageBitrateCoeff(1.0),
    m_dirty(false),
    m_valid(false),
    m_capabilities(0),
    m_storageManager(storageManager)
{
    m_capabilities |= QnAbstractStorageResource::cap::RemoveFile;
    m_capabilities |= QnAbstractStorageResource::cap::ListFile;
    m_capabilities |= QnAbstractStorageResource::cap::ReadFile;
}

QnFileStorageResource::~QnFileStorageResource()
{
#ifndef _WIN32
    if (!m_localPath.isEmpty())
    {
        umount(m_localPath.toLatin1().constData());
        rmdir(m_localPath.toLatin1().constData());
    }
#endif
}

bool QnFileStorageResource::removeFile(const QString& url)
{
    if (!initOrUpdate())
        return false;

    qnFileDeletor->deleteFile(removeProtocolPrefix(translateUrlToLocal(url)));
    return true;
}

bool QnFileStorageResource::renameFile(const QString& oldName, const QString& newName)
{
    if (!initOrUpdate())
        return false;

    return QFile::rename(translateUrlToLocal(oldName), translateUrlToLocal(newName));
}


bool QnFileStorageResource::removeDir(const QString& url)
{
    if (!initOrUpdate())
        return false;

    QDir dir;
    return dir.rmdir(removeProtocolPrefix(translateUrlToLocal(url)));
}

bool QnFileStorageResource::isDirExists(const QString& url)
{
    if (!initOrUpdate())
        return false;

    QDir d(translateUrlToLocal(url));
    return d.exists(removeProtocolPrefix(translateUrlToLocal(url)));
}

bool QnFileStorageResource::isFileExists(const QString& url)
{
    if (!initOrUpdate())
        return false;

    return QFile::exists(removeProtocolPrefix(translateUrlToLocal(url)));
}

qint64 QnFileStorageResource::getFreeSpace()
{
    if (!initOrUpdate())
        return -1;

    return getDiskFreeSpace(
        m_localPath.isEmpty() ?
        getPath() :
        m_localPath
    );
}

qint64 QnFileStorageResource::getTotalSpace()
{
    if (!initOrUpdate())
        return -1;

    return getDiskTotalSpace(
        m_localPath.isEmpty() ?
        getPath() :
        m_localPath
    );
}

QnAbstractStorageResource::FileInfoList QnFileStorageResource::getFileList(const QString& dirName)
{
    if (!initOrUpdate())
        return QnAbstractStorageResource::FileInfoList();

    QDir dir(translateUrlToLocal(dirName));

    QFileInfoList localList = dir.entryInfoList(
        QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot,
        QDir::DirsFirst
    );

    QnAbstractStorageResource::FileInfoList ret;

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
    if (!initOrUpdate())
        return -1;

    return QnFile::getFileSize(translateUrlToLocal(url));
}

bool QnFileStorageResource::isAvailable() const 
{
    if (!m_valid)
        m_dirty = true;

    if (!initOrUpdate())
        return false;

    if(!isStorageDirMounted())
        return false;

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
}

QString QnFileStorageResource::removeProtocolPrefix(const QString& url)
{
    int prefix = url.indexOf("://");
    return prefix == -1 ? url :QUrl(url).path().mid(1);
}

QnStorageResource* QnFileStorageResource::instance(const QString&)
{
    assert(QnStorageManager::instance());
    QnStorageResource* storage = new QnFileStorageResource(QnStorageManager::instance());
    storage->setSpaceLimit( MSSettings::roSettings()->value(nx_ms_conf::MIN_STORAGE_SPACE, nx_ms_conf::DEFAULT_MIN_STORAGE_SPACE).toLongLong() );
    return storage;
}

float QnFileStorageResource::getAvarageWritingUsage() const
{
    QueueFileWriter* writer = QnWriterPool::instance()->getWriter(getPath());
    return writer ? writer->getAvarageUsage() : 0;
}

float QnFileStorageResource::getStorageBitrateCoeff() const
{
    return m_storageBitrateCoeff;
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
    if (!m_localPath.isEmpty()) // smb
    {
        QUrl url(getUrl());

        QString cifsOptionsString =
            lit("username=%1,password=%2")
                .arg(url.userName())
                .arg(url.password());

        QString srcString = lit("//") + url.host() + url.path();

        auto randomString = []
        {
            std::stringstream randomStringStream;
            randomStringStream << std::hex << std::rand() << std::rand();

            return randomStringStream.str();
        };

        QString tmpPath = "/tmp/" + NX_TEMP_FOLDER_NAME + QString::fromStdString(randomString());
        rmdir(tmpPath.toLatin1().constData());

        mkdir(
            tmpPath.toLatin1().constData(),
            S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH
        );

        int retCode = mount(
            srcString.toLatin1().constData(),
            tmpPath.toLatin1().constData(),
            "cifs",
            MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_SILENT,
            cifsOptionsString.toLatin1().constData()
        );

        if (retCode == -1)
        {
            int err = errno;
            if (err != EBUSY)
            {
                rmdir(tmpPath.toLatin1().constData());
                m_dirty = true;
                return false;
            }
        }
        else
        {
            umount(tmpPath.toLatin1().constData());
            rmdir(tmpPath.toLatin1().constData());
        }
        return true;
    }

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

