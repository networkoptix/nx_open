#include "layout_storage_resource.h"
#include "recording/file_deletor.h"
#include "utils/common/util.h"
#include "utils/common/buffered_file.h"

class QnLayoutFile: public QFile
{
public:
    QnLayoutFile(QnLayoutFileStorageResource& storageResource, const QString& fileName):
        QFile(storageResource.getUrl()),
        m_storageResource(storageResource),
        m_fileOffset(0)
    {
        m_fileName = fileName;
    }

    virtual bool seek(qint64 offset) override
    {
        return QFile::seek(offset + m_fileOffset);
    }

    virtual bool open(QIODevice::OpenMode openMode) override
    {
        if (openMode & QIODevice::WriteOnly) {
            if (!m_storageResource.addFileEntry(m_fileName))
                return false;
        }

        bool rez = QFile::open(openMode);
        if (!rez)
            return rez;
        m_fileOffset = m_storageResource.getFileOffset(m_fileName);

        return rez;
    }

private:
    QString m_fileName;
    qint64 m_fileOffset;
    QnLayoutFileStorageResource& m_storageResource;
};

QIODevice* QnLayoutFileStorageResource::open(const QString& url, QIODevice::OpenMode openMode)
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



QnLayoutFileStorageResource::QnLayoutFileStorageResource()
{
};

bool QnLayoutFileStorageResource::isNeedControlFreeSpace()
{
    return false;
}

bool QnLayoutFileStorageResource::removeFile(const QString& url)
{
    qnFileDeletor->deleteFile(removeProtocolPrefix(url));
    return true;
}

bool QnLayoutFileStorageResource::renameFile(const QString& oldName, const QString& newName)
{
    return QFile::rename(oldName, newName);
}


bool QnLayoutFileStorageResource::removeDir(const QString& url)
{
    qnFileDeletor->deleteDir(removeProtocolPrefix(url));
    return true;
}

bool QnLayoutFileStorageResource::isDirExists(const QString& url)
{
    QDir d(url);
    return d.exists(removeProtocolPrefix(url));
}

bool QnLayoutFileStorageResource::isFileExists(const QString& url)
{
    return QFile::exists(removeProtocolPrefix(url));
}

qint64 QnLayoutFileStorageResource::getFreeSpace()
{
    return getDiskFreeSpace(removeProtocolPrefix(getUrl()));
}

QFileInfoList QnLayoutFileStorageResource::getFileList(const QString& dirName)
{
    QDir dir;
    dir.cd(dirName);
    return dir.entryInfoList(QDir::Files);
}


bool QnLayoutFileStorageResource::isStorageAvailable()
{
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
        else 
            return false;
    }

    return false;
}

int QnLayoutFileStorageResource::getChunkLen() const 
{
    return 60;
}

QString QnLayoutFileStorageResource::removeProtocolPrefix(const QString& url)
{
    int prefix = url.indexOf("://");
    return prefix == -1 ? url : url.mid(prefix + 3);
}

QnStorageResource* QnLayoutFileStorageResource::instance()
{
    return new QnLayoutFileStorageResource();
}

bool QnLayoutFileStorageResource::isStorageAvailableForWriting()
{
    return false; // it is read only file system
}

void QnLayoutFileStorageResource::readIndexHeader()
{
    QFile file(getUrl());
    if (file.open(QIODevice::ReadOnly))
        file.read((char*) &m_index, sizeof(m_index));
}

bool QnLayoutFileStorageResource::addFileEntry(const QString& fileName)
{
    if (m_index.entryCount >= MAX_FILES_AT_LAYOUT)
        return false;

    QFile file(getUrl());
    qint64 fileSize = file.size();
    if (fileSize > 0)
        readIndexHeader();
    else
        fileSize = sizeof(m_index);

    m_index.entries[m_index.entryCount++] = QnLayoutFileIndexEntry(fileSize, qHash(fileName));

    if (file.open(QIODevice::WriteOnly))
        file.write((const char*) &m_index, sizeof(m_index));
    file.seek(fileSize);
    QByteArray utf8FileName = fileName.toUtf8();
    file.write(utf8FileName);

    return true;
}

qint64 QnLayoutFileStorageResource::getFileOffset(const QString& fileName)
{
    QFile file(getUrl());
    if (!file.open(QIODevice::ReadOnly))
        return -1;

    quint32 hash = qHash(fileName);
    for (int i = 0; i < m_index.entryCount; ++i)
    {
        if (m_index.entries[i].fileNameCrc == hash)
        {
            file.seek(m_index.entries[i].offset);
            char tmpBuffer[1024]; // buffer size is max file len
            int readed = file.read(tmpBuffer, sizeof(tmpBuffer));
            if (fileName == QString::fromUtf8(tmpBuffer, readed))
                return m_index.entries[i].offset;
        }
    }
    return -1;
}
