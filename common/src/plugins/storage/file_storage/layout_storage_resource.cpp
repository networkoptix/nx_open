#include "layout_storage_resource.h"
#include "recording/file_deletor.h"
#include "utils/common/util.h"
#include "plugins/resources/archive/filetypesupport.h"

class QnLayoutFile: public QIODevice
{
public:
    QnLayoutFile(QnLayoutFileStorageResource& storageResource, const QString& fileName):
        m_file(QnLayoutFileStorageResource::removeProtocolPrefix(storageResource.getUrl())),
        m_storageResource(storageResource),
        m_fileOffset(0),
        m_fileSize(0),
        m_mutex(QMutex::Recursive),
        m_storedPosition(0),
        m_openMode(QIODevice::NotOpen)
    {
        m_fileName = fileName.mid(fileName.lastIndexOf(QLatin1Char('?'))+1);
    }

    virtual ~QnLayoutFile()
    {
        close();
    }

    virtual bool seek(qint64 offset) override
    {
        QMutexLocker lock(&m_mutex);
        int rez = m_file.seek(offset + m_fileOffset);
        QIODevice::seek(offset);
        return rez;
    }

    virtual qint64 size() const
    {
        QMutexLocker lock(&m_mutex);
        return m_fileSize;   
    }

    virtual qint64 pos() const
    {
        QMutexLocker lock(&m_mutex);
        return m_file.pos() - m_fileOffset;
    }

    virtual qint64 readData(char *data, qint64 maxSize) override
    {
        QMutexLocker lock(&m_mutex);
        return m_file.read(data, qMin(m_fileSize - pos(), maxSize));
    }

    virtual qint64 writeData(const char *data, qint64 maxSize) override
    {
        QMutexLocker lock(&m_mutex);
        qint64 rez = m_file.write(data, maxSize);
        if (rez > 0)
            m_fileSize = qMax(m_fileSize, m_file.pos() - m_fileOffset);
        return rez;
    }

    virtual void close() override
    {
        QMutexLocker lock(&m_mutex);
        if ((m_openMode & QIODevice::WriteOnly) && m_storageResource.m_novFileOffset > 0)
        {
            seek(size());
            m_storageResource.addBinaryPostfix(m_file);
        }
        m_file.close();
        m_storageResource.unregisterFile(this);
    }

    virtual bool open(QIODevice::OpenMode openMode) override
    {
        QMutexLocker lock(&m_mutex);
        m_openMode = openMode;
        if (openMode & QIODevice::WriteOnly) 
        {
            if (!m_storageResource.addFileEntry(m_fileName))
                return false;
            openMode |= QIODevice::Append;
        }
        m_file.setFileName(QnLayoutFileStorageResource::removeProtocolPrefix(m_storageResource.getUrl()));
        if (!m_file.open(openMode))
            return false;
        m_fileOffset = m_storageResource.getFileOffset(m_fileName, &m_fileSize);
        if (m_fileOffset == -1)
            return false;
        m_file.seek(m_fileOffset);
        return QIODevice::open(openMode);
    }

    void lockFile()
    {
        m_mutex.lock();
    }

    void unlockFile()
    {
        m_mutex.unlock();
    }

    void storeStateAndClose()
    {
        m_storedPosition = pos();
        m_file.close();
    }

    void restoreState()
    {
        open(m_openMode);
        seek(m_storedPosition);
    }

private:
    QFile m_file;
    mutable QMutex m_mutex;
    QnLayoutFileStorageResource& m_storageResource;

    qint64 m_fileOffset;
    qint64 m_fileSize;
    QString m_fileName;
    qint64 m_storedPosition;
    QIODevice::OpenMode m_openMode;
};

// -------------------------------- QnLayoutFileStorageResource ---------------------------

QMutex QnLayoutFileStorageResource::m_storageSync;
QSet<QnLayoutFileStorageResource*> QnLayoutFileStorageResource::m_allStorages;

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
    registerFile(rez);
    return rez;
}

void QnLayoutFileStorageResource::registerFile(QnLayoutFile* file)
{
    QMutexLocker lock(&m_fileSync);
    m_openedFiles.insert(file);
}

void QnLayoutFileStorageResource::unregisterFile(QnLayoutFile* file)
{
    QMutexLocker lock(&m_fileSync);
    m_openedFiles.remove(file);
}

void QnLayoutFileStorageResource::closeOpenedFiles()
{
    QMutexLocker lock(&m_fileSync);
    for (QSet<QnLayoutFile*>::Iterator itr = m_openedFiles.begin(); itr != m_openedFiles.end(); ++itr)
    {
        QnLayoutFile* file = *itr;
        file->lockFile();
        file->storeStateAndClose();
    }
    m_index.entryCount = 0;
}

void QnLayoutFileStorageResource::restoreOpenedFiles()
{
    QMutexLocker lock(&m_fileSync);
    for (QSet<QnLayoutFile*>::Iterator itr = m_openedFiles.begin(); itr != m_openedFiles.end(); ++itr)
    {
        QnLayoutFile* file = *itr;
        file->restoreState();
        file->unlockFile();
    }
}

QnLayoutFileStorageResource::QnLayoutFileStorageResource():
    m_fileSync(QMutex::Recursive)
{
    QMutexLocker lock(&m_storageSync);
    m_novFileOffset = 0;
    //m_novFileLen = 0;
    m_allStorages.insert(this);
}

QnLayoutFileStorageResource::~QnLayoutFileStorageResource()
{
    QMutexLocker lock(&m_storageSync);
    m_allStorages.remove(this);
}


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
    QMutexLocker lock(&m_storageSync);
    for (QSet<QnLayoutFileStorageResource*>::Iterator itr = m_allStorages.begin(); itr != m_allStorages.end(); ++itr) 
    {
        QnLayoutFileStorageResource* storage = *itr;
        QString storageUrl = removeProtocolPrefix(storage->getUrl());
        if (storageUrl == removeProtocolPrefix(newName) || storageUrl == removeProtocolPrefix(oldName))
            storage->closeOpenedFiles();
    }
    QFile::remove(removeProtocolPrefix(newName));
    bool rez = QFile::rename(removeProtocolPrefix(oldName), removeProtocolPrefix(newName));
    for (QSet<QnLayoutFileStorageResource*>::Iterator itr = m_allStorages.begin(); itr != m_allStorages.end(); ++itr) 
    {
        QnLayoutFileStorageResource* storage = *itr;
        QString storageUrl = removeProtocolPrefix(storage->getUrl());
        if (storageUrl == removeProtocolPrefix(newName))
            storage->restoreOpenedFiles();
        else if (storageUrl == removeProtocolPrefix(oldName)) {
            storage->setUrl(newName);
            storage->restoreOpenedFiles();
        }
    }
    if (rez)
        setUrl(newName);
    return rez;
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

bool QnLayoutFileStorageResource::isCatalogAccessible()
{
    return true;
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
    {
        file.seek(m_novFileOffset);
        file.read((char*) &m_index, sizeof(m_index));
    }
}

int QnLayoutFileStorageResource::getPostfixSize() const
{
    return m_novFileOffset == 0 ? 0 : sizeof(qint64)*2;
}

bool QnLayoutFileStorageResource::addFileEntry(const QString& srcFileName)
{
    QMutexLocker lock(&m_fileSync);

    QString fileName = srcFileName.mid(srcFileName.lastIndexOf(L'?')+1);
    if (m_index.entryCount >= (quint32)MAX_FILES_AT_LAYOUT)
        return false;

    QFile file(removeProtocolPrefix(getUrl()));
    qint64 fileSize = file.size() -  getPostfixSize();
    if (fileSize > 0)
        readIndexHeader();
    else
        fileSize = sizeof(m_index);

    m_index.entries[m_index.entryCount++] = QnLayoutFileIndexEntry(fileSize - m_novFileOffset, qHash(fileName));

    if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
        return false;
    file.seek(m_novFileOffset);
    file.write((const char*) &m_index, sizeof(m_index));
    file.seek(fileSize);
    QByteArray utf8FileName = fileName.toUtf8();
    utf8FileName.append('\0');
    file.write(utf8FileName);
    if (m_novFileOffset > 0)
        addBinaryPostfix(file);
    return true;
}

qint64 QnLayoutFileStorageResource::getFileOffset(const QString& fileName, qint64* fileSize)
{
    QMutexLocker lock(&m_fileSync);

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
            file.seek(m_index.entries[i].offset + m_novFileOffset);
            char tmpBuffer[1024]; // buffer size is max file len
            int readed = file.read(tmpBuffer, sizeof(tmpBuffer));
            QByteArray readedFileName(tmpBuffer, qMin(readed, utf8FileName.length()));
            if (utf8FileName == readedFileName)
            {
                qint64 offset = m_index.entries[i].offset + m_novFileOffset + fileName.toUtf8().length()+1;
                if (i < m_index.entryCount-1)
                    *fileSize = m_index.entries[i+1].offset + m_novFileOffset - offset;
                else {
                    qint64 endPos = file.size() - getPostfixSize();
                    *fileSize = endPos - offset;
                }
                return offset;
            }
        }
    }
    return -1;
}

void QnLayoutFileStorageResource::setUrl(const QString& value)
{
    QnStorageResource::setUrl(value);
    if (value.endsWith(QLatin1String(".exe")) || value.endsWith(QLatin1String(".exe.tmp")))
    {
        // find nov file inside binary
        QFile f(removeProtocolPrefix(value));
        if (f.open(QIODevice::ReadOnly))
        {
            qint64 postfixPos = f.size() - sizeof(quint64) * 2;
            f.seek(postfixPos); // go to nov index and magic
            qint64 novOffset;
            quint64 magic;
            f.read((char*) &novOffset, sizeof(qint64));
            f.read((char*) &magic, sizeof(qint64));
            if (magic == FileTypeSupport::NOV_EXE_MAGIC) 
            {
                m_novFileOffset = novOffset;
                //m_novFileLen = f.size() - m_novFileOffset - sizeof(qint64)*2;
            }
        }
    }
}

void QnLayoutFileStorageResource::addBinaryPostfix(QFile& file)
{
    file.write((char*) &m_novFileOffset, sizeof(qint64));

    const quint64 magic = FileTypeSupport::NOV_EXE_MAGIC;
    file.write((char*) &magic, sizeof(qint64));
}
