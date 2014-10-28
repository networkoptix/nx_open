#include "qtfile_storage_resource.h"

#include <QtCore/QDir>

#include "utils/common/util.h"


QIODevice* QnQtFileStorageResource::open(const QString& url, QIODevice::OpenMode openMode)
{
    QString fileName = removeProtocolPrefix(url);

    QFile* rez = new QFile(fileName);
    if (!rez->open(openMode))
    {
        delete rez;
        return 0;
    }
    return rez;
}



QnQtFileStorageResource::QnQtFileStorageResource()
{
};

bool QnQtFileStorageResource::isNeedControlFreeSpace()
{
    return false;
}

bool QnQtFileStorageResource::removeFile(const QString& url)
{
    return QFile::remove(removeProtocolPrefix(url));
}

bool QnQtFileStorageResource::renameFile(const QString& oldName, const QString& newName)
{
    return QFile::rename(oldName, newName);
}


bool QnQtFileStorageResource::removeDir(const QString& url)
{
    QDir dir(removeProtocolPrefix(url));
    QList<QFileInfo> list = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for(const QFileInfo& fi: list)
        removeFile(fi.absoluteFilePath());
    return true;
}

bool QnQtFileStorageResource::isDirExists(const QString& url)
{
    QDir d(url);
    return d.exists(removeProtocolPrefix(url));
}

bool QnQtFileStorageResource::isCatalogAccessible()
{
    return true;
}

bool QnQtFileStorageResource::isFileExists(const QString& url)
{
    return QFile::exists(removeProtocolPrefix(url));
}

qint64 QnQtFileStorageResource::getFreeSpace()
{
    return getDiskFreeSpace(removeProtocolPrefix(getUrl()));
}

qint64 QnQtFileStorageResource::getTotalSpace()
{
    return getDiskTotalSpace(removeProtocolPrefix(getUrl()));
}

QFileInfoList QnQtFileStorageResource::getFileList(const QString& dirName)
{
    QDir dir;
    if (dir.cd(dirName))
        return dir.entryInfoList(QDir::Files);
    else
        return QFileInfoList();
}

qint64 QnQtFileStorageResource::getFileSize(const QString& url) const
{
    Q_UNUSED(url)
	return 0; // not implemented
}

bool QnQtFileStorageResource::isStorageAvailable()
{
    QString tmpDir = closeDirPath(getUrl()) + QLatin1String("tmp") + QString::number(qrand());
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
        else 
            return false;
    }

    return false;
}

int QnQtFileStorageResource::getChunkLen() const 
{
    return 60;
}

QString QnQtFileStorageResource::removeProtocolPrefix(const QString& url)
{
    int prefix = url.indexOf(QLatin1String("://"));
    return prefix == -1 ? url : url.mid(prefix + 3);
}

QnStorageResource* QnQtFileStorageResource::instance()
{
    return new QnQtFileStorageResource();
}

bool QnQtFileStorageResource::isStorageAvailableForWriting()
{
    return false; // it is read only file system
}
