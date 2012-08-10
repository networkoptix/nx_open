#include "layout_storage_resource.h"
#include "recording/file_deletor.h"
#include "utils/common/util.h"

class QnLayoutFile: public QIODevice
{
public:
    QnLayoutFile(QnLayoutFileStorageResource& storageResource, const QString& fileName):
        m_file(QnLayoutFileStorageResource::removeProtocolPrefix(storageResource.getUrl())),
        m_storageResource(storageResource),
        m_fileOffset(0),
        m_fileSize(0)
    {
        m_fileName = fileName.mid(fileName.indexOf(QLatin1Char('?'))+1);
    }

    virtual bool seek(qint64 offset) override
    {
        return m_file.seek(offset + m_fileOffset);
    }

    virtual qint64 size() const
    {
        return m_fileSize;   
    }

    virtual qint64 pos() const
    {
        return m_file.pos() - m_fileOffset;
    }

    virtual qint64 readData(char *data, qint64 maxSize) override
    {
        return m_file.read(data, qMin(m_fileSize - pos(), maxSize));
    }

    virtual qint64 writeData(const char *data, qint64 maxSize) override
    {
        return m_file.write(data, maxSize);
    }

    virtual bool open(QIODevice::OpenMode openMode) override
    {
        if (openMode & QIODevice::WriteOnly) 
        {
            if (!m_storageResource.addFileEntry(m_fileName))
                return false;
            openMode |= QIODevice::Append;
        }

        if (!m_file.open(openMode))
            return false;
        m_fileOffset = m_storageResource.getFileOffset(m_fileName, &m_fileSize);
        if (m_fileOffset == -1)
            return false;
        m_file.seek(m_fileOffset);

        return QIODevice::open(openMode);
    }

private:
    QFile m_file;
    qint64 m_fileSize;
    QString m_fileName;
    qint64 m_fileOffset;
    QnLayoutFileStorageResource& m_storageResource;
};

QIODevice* QnLayoutFileStorageResource::open(const QString& url, QIODevice::OpenMode openMode)
{
    if (getUrl().isEmpty()) {
        int postfixPos = url.indexOf(QLatin1Char('?'));
        if (postfixPos == -1)
            setUrl(url);
        else
            setUrl(url.left(postfixPos));
    }
    QnLayoutFile* rez = new QnLayoutFile(*this, url);
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

qint64 QnLayoutFileStorageResource::getFileSize(const QString& fillName) const
{
    Q_UNUSED(fillName)
    return 0; // not implemented
}

bool QnLayoutFileStorageResource::isStorageAvailable()
{
    QString tmpDir = closeDirPath(removeProtocolPrefix(getUrl())) + QLatin1String("tmp") + QString::number(rand());
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
    int prefix = url.indexOf(QLatin1String("://"));
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
    QFile file(removeProtocolPrefix(getUrl()));
    if (file.open(QIODevice::ReadOnly))
        file.read((char*) &m_index, sizeof(m_index));
}

bool QnLayoutFileStorageResource::addFileEntry(const QString& fileName)
{
    if (m_index.entryCount >= (quint32)MAX_FILES_AT_LAYOUT)
        return false;

    QFile file(removeProtocolPrefix(getUrl()));
    qint64 fileSize = file.size();
    if (fileSize > 0)
        readIndexHeader();
    else
        fileSize = sizeof(m_index);

    m_index.entries[m_index.entryCount++] = QnLayoutFileIndexEntry(fileSize, qHash(fileName));

    if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
        return false;
    file.seek(0);
    file.write((const char*) &m_index, sizeof(m_index));
    file.seek(fileSize);
    QByteArray utf8FileName = fileName.toUtf8();
    utf8FileName.append('\0');
    file.write(utf8FileName);

    return true;
}

qint64 QnLayoutFileStorageResource::getFileOffset(const QString& fileName, qint64* fileSize)
{
    *fileSize = 0;
    if (m_index.entryCount == 0)
        readIndexHeader();

    QFile file(removeProtocolPrefix(getUrl()));
    if (!file.open(QIODevice::ReadOnly))
        return -1;

    quint32 hash = qHash(fileName);
    QByteArray utf8FileName = fileName.toUtf8();
    for (uint i = 0; i < m_index.entryCount; ++i)
    {
        if (m_index.entries[i].fileNameCrc == hash)
        {
            file.seek(m_index.entries[i].offset);
            char tmpBuffer[1024]; // buffer size is max file len
            int readed = file.read(tmpBuffer, sizeof(tmpBuffer));
            QByteArray readedFileName(tmpBuffer, qMin(readed, utf8FileName.length()));
            if (utf8FileName == readedFileName) 
            {
                qint64 offset = m_index.entries[i].offset + fileName.toUtf8().length()+1;
                if (i < m_index.entryCount-1)
                    *fileSize = m_index.entries[i+1].offset - offset;
                else
                    *fileSize = file.size() - offset;
                return offset;
            }
        }
    }
    return -1;
}
