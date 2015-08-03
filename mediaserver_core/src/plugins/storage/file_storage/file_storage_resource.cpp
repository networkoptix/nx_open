
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

#ifdef WIN32
#pragma comment(lib, "mpr.lib")
#endif

QIODevice* QnFileStorageResource::open(const QString& url, QIODevice::OpenMode openMode)
{
    QString fileName = removeProtocolPrefix(url);

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
        return QUrl(url).path().mid(1);
}

bool QnFileStorageResource::updatePermissions() const
{
    QnMutexLocker lock( &m_mutexPermission );
    if (m_durty) {
        m_durty = false;
        QUrl storageUrl(getUrl());
        QString path = storageUrl.path().mid((1));
#ifdef WIN32
        if (path.startsWith("\\\\") && !storageUrl.userName().isEmpty())
        {
            NETRESOURCE netRes;
            memset(&netRes, 0, sizeof(netRes));
            netRes.dwType = RESOURCETYPE_DISK;
            netRes.lpRemoteName = (LPWSTR) path.constData();
            LPWSTR password = (LPWSTR) storageUrl.password().constData();
            LPWSTR user = (LPWSTR) storageUrl.userName().constData();
            if (WNetUseConnection(0, &netRes, password, user, 0, 0, 0, 0) != NO_ERROR)
                return false;
        }
#endif
    }
    return true;
}


bool QnFileStorageResource::checkWriteCap() const
{
    if( !isStorageDirMounted() )
        return false;
    
    if (!updatePermissions())
    	return false;
    
    if (hasFlags(Qn::deprecated))
        return false;
    
    QDir dir(getPath());
    
    bool needRemoveDir = false;
    if (!dir.exists())  {
        if (!dir.mkpath(getPath()))
            return false;
        needRemoveDir = true;
    }
    
    QFile file(closeDirPath(getPath()) + QString("tmp") + QString::number((unsigned) ((rand() << 16) + rand())));
    bool result = file.open(QFile::WriteOnly);
    if (result) {
        file.close();
        file.remove();
    }
    
    if (needRemoveDir)
        dir.remove(getPath());
    
    return result;
}

bool QnFileStorageResource::checkDBCap() const
{
#ifdef _WIN32
    return true;
#else
    QString storagePath = getPath();

    QList<QnPlatformMonitor::PartitionSpace> partitions = 
        qnPlatform->monitor()->QnPlatformMonitor::totalPartitionSpaceInfo(
            QnPlatformMonitor::NetworkPartition );

    for(const QnPlatformMonitor::PartitionSpace& partition: partitions)
    {
        if(storagePath.startsWith(partition.path))
            return false;
    }
    return true;
#endif
}

int QnFileStorageResource::getCapabilities() const
{
    return m_capabilities | (checkWriteCap() ? QnAbstractStorageResource::cap::WriteFile : 0);
}

void QnFileStorageResource::setUrl(const QString& url)
{
    QnMutexLocker lock( &m_mutex );
    QnStorageResource::setUrl(url);
    m_durty = true;
}

QnFileStorageResource::QnFileStorageResource():
    m_storageBitrateCoeff(1.0),
    m_durty(false),
    m_capabilities(0)
{
    m_capabilities |= QnAbstractStorageResource::cap::RemoveFile;
    m_capabilities |= QnAbstractStorageResource::cap::ListFile;
    m_capabilities |= QnAbstractStorageResource::cap::ReadFile;

    if (checkDBCap())
        m_capabilities |= QnAbstractStorageResource::cap::DBReady;
};

QnFileStorageResource::~QnFileStorageResource()
{

}

bool QnFileStorageResource::removeFile(const QString& url)
{
    updatePermissions();
    QFile file(url);
    qnFileDeletor->deleteFile(removeProtocolPrefix(url));
    return true;
}

bool QnFileStorageResource::renameFile(const QString& oldName, const QString& newName)
{
    updatePermissions();
    return QFile::rename(oldName, newName);
}


bool QnFileStorageResource::removeDir(const QString& url)
{
    updatePermissions();
    QDir dir;
    return dir.rmdir(removeProtocolPrefix(url));
}

bool QnFileStorageResource::isDirExists(const QString& url)
{
    updatePermissions();
    QDir d(url);
    return d.exists(removeProtocolPrefix(url));
}

bool QnFileStorageResource::isFileExists(const QString& url)
{
    updatePermissions();
    return QFile::exists(removeProtocolPrefix(url));
}

qint64 QnFileStorageResource::getFreeSpace()
{
    if (!updatePermissions())
        return -1;
    return getDiskFreeSpace(getPath());
}

qint64 QnFileStorageResource::getTotalSpace()
{
    if (!updatePermissions())
        return -1;
    return getDiskTotalSpace(getPath());
}

QnAbstractStorageResource::FileInfoList QnFileStorageResource::getFileList(const QString& dirName)
{
    updatePermissions();
    QDir dir(dirName);
    return QnAbstractStorageResource::FIListFromQFIList(
        dir.entryInfoList(
            QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot,
            QDir::DirsFirst
        )
    );
}

qint64 QnFileStorageResource::getFileSize(const QString& url) const
{
    updatePermissions();
    return QnFile::getFileSize(url);
}

bool QnFileStorageResource::isAvailable() const 
{
    if( !isStorageDirMounted() )
        return false;

    if (!updatePermissions())
		return false;

    QString tmpDir = closeDirPath(getPath()) + QString("tmp") + QString::number(rand());
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
    return prefix == -1 ? url :QUrl(url).path().mid(1);//url.mid(prefix + 3);
}

QnStorageResource* QnFileStorageResource::instance(const QString&)
{
    QnStorageResource* storage = new QnFileStorageResource();
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
    const QString& notResolvedStoragePath = closeDirPath(getPath());
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

