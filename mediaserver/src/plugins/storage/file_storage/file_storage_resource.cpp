
#include <iostream>

#include <QtCore/QDir>

#include "file_storage_resource.h"
#include "utils/common/util.h"
#include "utils/common/buffered_file.h"
#include "recorder/file_deletor.h"

#ifdef Q_OS_WIN
#include "windows.h"
#endif
#include <media_server/settings.h>
#include <recorder/storage_manager.h>


static const int FFMPEG_BUFFER_SIZE = 1024*1024*4;

QIODevice* QnFileStorageResource::open(const QString& url, QIODevice::OpenMode openMode)
{
    QString fileName = removeProtocolPrefix(url);
    int ioBlockSize = 0;
    int ffmpegBufferSize = 0;

    int systemFlags = 0;
    if (openMode & QIODevice::WriteOnly) {
        ioBlockSize = IO_BLOCK_SIZE;
        ffmpegBufferSize = FFMPEG_BUFFER_SIZE;
#ifdef Q_OS_WIN
        if (MSSettings::roSettings()->value("disableDirectIO").toInt() != 1)
            systemFlags = FILE_FLAG_NO_BUFFERING;
#endif
    }
    std::unique_ptr<QBufferedFile> rez( new QBufferedFile(fileName, ioBlockSize, ffmpegBufferSize) );
    rez->setSystemFlags(systemFlags);
    if (!rez->open(openMode))
        return 0;
    return rez.release();
}

QnFileStorageResource::QnFileStorageResource():
    m_storageBitrateCoeff(1.0)
{

};

QnFileStorageResource::~QnFileStorageResource()
{

}

bool QnFileStorageResource::isNeedControlFreeSpace()
{
    return true;
}

bool QnFileStorageResource::removeFile(const QString& url)
{
    QFile file(url);
    qnFileDeletor->deleteFile(removeProtocolPrefix(url));
    return true;
}

bool QnFileStorageResource::renameFile(const QString& oldName, const QString& newName)
{
    return QFile::rename(oldName, newName);
}


bool QnFileStorageResource::removeDir(const QString& url)
{
    qnFileDeletor->deleteDir(removeProtocolPrefix(url));
    return true;
}

bool QnFileStorageResource::isDirExists(const QString& url)
{
    QDir d(url);
    return d.exists(removeProtocolPrefix(url));
}

bool QnFileStorageResource::isCatalogAccessible()
{
    return true;
}

bool QnFileStorageResource::isFileExists(const QString& url)
{
    return QFile::exists(removeProtocolPrefix(url));
}

qint64 QnFileStorageResource::getFreeSpace()
{
    return getDiskFreeSpace(removeProtocolPrefix(getUrl()));
}

qint64 QnFileStorageResource::getTotalSpace()
{
    return getDiskTotalSpace(removeProtocolPrefix(getUrl()));
}

QFileInfoList QnFileStorageResource::getFileList(const QString& dirName)
{
    QDir dir(dirName);
    return dir.entryInfoList(QDir::Files);
}

qint64 QnFileStorageResource::getFileSize(const QString& url) const
{
    return QnFile::getFileSize(url);
}

bool QnFileStorageResource::isStorageAvailableForWriting()
{
    if( !isStorageDirMounted() )
        return false;

    if (hasFlags(deprecated))
        return false;

    QDir dir(getUrl());

    bool needRemoveDir = false;
    if (!dir.exists())
    {
        if (!dir.mkpath(getUrl()))
            return false;
        needRemoveDir = true;
    }

    QFile file(closeDirPath(getUrl()) + QString("tmp") + QString::number((unsigned) ((rand() << 16) + rand())));
    bool result = file.open(QFile::WriteOnly);
    if (result)
    {
        file.close();
        file.remove();
    }

    if (needRemoveDir)
        dir.remove(getUrl());

    return result;
}

bool QnFileStorageResource::isStorageAvailable()
{
    if( !isStorageDirMounted() )
        return false;

    QString tmpDir = closeDirPath(getUrl()) + QString("tmp") + QString::number(rand());
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

int QnFileStorageResource::getChunkLen() const 
{
    return 60;
}

QString QnFileStorageResource::removeProtocolPrefix(const QString& url)
{
    int prefix = url.indexOf("://");
    return prefix == -1 ? url : url.mid(prefix + 3);
}

QnStorageResource* QnFileStorageResource::instance()
{
    QnStorageResource* storage = new QnFileStorageResource();
    storage->setSpaceLimit( MSSettings::roSettings()->value(nx_ms_conf::MIN_STORAGE_SPACE, nx_ms_conf::DEFAULT_MIN_STORAGE_SPACE).toLongLong() );
    return storage;
}

float QnFileStorageResource::getAvarageWritingUsage() const
{
    QueueFileWriter* writer = QnWriterPool::instance()->getWriter(getUrl());
    return writer ? writer->getAvarageUsage() : 0;
}

void QnFileStorageResource::setStorageBitrateCoeff(float value)
{
    qDebug() << "QnFileStorageResource " << getUrl() << "coeff " << value;
    m_storageBitrateCoeff = value;
}

float QnFileStorageResource::getStorageBitrateCoeff() const
{
    return m_storageBitrateCoeff;
}

bool QnFileStorageResource::isStorageDirMounted()
{
#ifdef _WIN32
    return true;    //common check is enough on mswin
#else
    //on unix, checking that storage directory is mounted, if it is to be mounted
    const QString& storagePath = QDir(closeDirPath(getUrl())).canonicalPath();   //following symbolic link

    QStringList mountPoints;
    if( !readTabFile( lit("/etc/fstab"), &mountPoints ) )
    {
        NX_LOG( lit("Could not read /etc/fstab file while checking storage %1 availability").arg(storagePath), cl_logWARNING );
        return false;
    }

    //checking if storage dir is to be mounted
    QString storageMountPoint;
    foreach( QString mountPoint, mountPoints )
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

    return true;
#endif    // _WIN32
}

bool QnFileStorageResource::readTabFile( const QString& filePath, QStringList* const mountPoints )
{
    QFile tabFile( filePath );
    if( !tabFile.open(QIODevice::ReadOnly) )
        return false;

    while( !tabFile.atEnd() )
    {
        QString line = QString::fromLatin1(tabFile.readLine().trimmed());
        if( line.isEmpty() || line.startsWith('#') )
            continue;

        const QStringList& fields = line.split( QRegExp(QLatin1String("[\\s\\t]+")), QString::SkipEmptyParts );
        if( fields.size() >= 2 )
            mountPoints->push_back( closeDirPath( fields[1] ) );
    }

    return true;
}
