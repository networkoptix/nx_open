#include "qtfile_storage_resource.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QDir>

#include <nx/utils/random.h>
#include <utils/common/util.h>

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

int QnQtFileStorageResource::getCapabilities() const
{
    return m_capabilities;
}

QnQtFileStorageResource::QnQtFileStorageResource(QnCommonModule* commonModule):
    base_type(commonModule),
    m_capabilities(0)
{
    m_capabilities |= cap::ListFile;
    m_capabilities |= cap::ReadFile;
};

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

bool QnQtFileStorageResource::isFileExists(const QString& url)
{
    return QFile::exists(removeProtocolPrefix(url));
}

qint64 QnQtFileStorageResource::getFreeSpace()
{
    return getDiskFreeSpace(removeProtocolPrefix(getUrl()));
}

qint64 QnQtFileStorageResource::getTotalSpace() const
{
    return getDiskTotalSpace(removeProtocolPrefix(getUrl()));
}

QnAbstractStorageResource::FileInfoList QnQtFileStorageResource::getFileList(const QString& dirName)
{
    QDir dir;
    if (dir.cd(dirName))
        return QnAbstractStorageResource::FIListFromQFIList(dir.entryInfoList(QDir::Files));
    else
        return QnAbstractStorageResource::FileInfoList();
}

qint64 QnQtFileStorageResource::getFileSize(const QString& url) const
{
    Q_UNUSED(url)
	return 0; // not implemented
}

Qn::StorageInitResult QnQtFileStorageResource::initOrUpdate()
{
    QString tmpDir = closeDirPath(getUrl()) + QLatin1String("tmp")
        + QString::number(nx::utils::random::number<uint>());

    QDir dir(tmpDir);
    if (dir.exists()) {
        dir.remove(tmpDir);
        return Qn::StorageInit_Ok;
    }
    else {
        if (dir.mkpath(tmpDir))
        {
            dir.rmdir(tmpDir);
            return Qn::StorageInit_Ok;
        }
        else
            return Qn::StorageInit_WrongPath;
    }

    return Qn::StorageInit_WrongPath;
}

QString QnQtFileStorageResource::removeProtocolPrefix(const QString& url) const
{
    int prefix = url.indexOf(QLatin1String("://"));
    return prefix == -1 ? url : url.mid(prefix + 3);
}

QnStorageResource* QnQtFileStorageResource::instance(QnCommonModule* commonModule, const QString&)
{
    return new QnQtFileStorageResource(commonModule);
}

#endif //ENABLE_DATA_PROVIDERS
