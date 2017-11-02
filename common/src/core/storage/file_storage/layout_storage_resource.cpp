#include "layout_storage_resource.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>

#include <nx/utils/random.h>
#include <core/resource/avi/filetypesupport.h>
#include <recording/time_period_list.h>
#include <utils/common/util.h>
#include <utils/fs/file.h>

namespace {

    /* Maximum nov-file entries. */
    const quint32 kMaxEntries = 1024;

    /* Max future nov-file version that should be opened by the current client version. */
    const quint32 kMaxVersion = 1024;

    /* Protocol for items on the exported layouts. */
    static const QString kLayoutProtocol(lit("layout://"));
}

class QnLayoutFile: public QIODevice
{
public:
    QnLayoutFile(QnLayoutFileStorageResource& storageResource, const QString& fileName):
        m_file(storageResource.getUrl()),
        m_mutex(QnMutex::Recursive),
        m_storageResource(storageResource),
        m_fileOffset(0),
        m_fileSize(0),
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
        QnMutexLocker lock( &m_mutex );
        int rez = m_file.seek(offset + m_fileOffset);
        QIODevice::seek(offset);
        return rez;
    }

    virtual qint64 size() const
    {
        QnMutexLocker lock( &m_mutex );
        return m_fileSize;
    }

    virtual qint64 pos() const
    {
        QnMutexLocker lock( &m_mutex );
        return m_file.pos() - m_fileOffset;
    }

    virtual qint64 readData(char *data, qint64 maxSize) override
    {
        QnMutexLocker lock( &m_mutex );
        return m_file.read(data, qMin(m_fileSize - pos(), maxSize));
    }

    virtual qint64 writeData(const char *data, qint64 maxSize) override
    {
        QnMutexLocker lock( &m_mutex );
        qint64 rez = m_file.write(data, maxSize);
        if (rez > 0)
            m_fileSize = qMax(m_fileSize, m_file.pos() - m_fileOffset);
        return rez;
    }

    virtual void close() override
    {
        QnMutexLocker lock( &m_mutex );
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
        QnMutexLocker lock( &m_mutex );
        m_openMode = openMode;
        if (openMode & QIODevice::WriteOnly)
        {
            qint64 fileSize;
            if (m_storageResource.getFileOffset(m_fileName, &fileSize) == -1)
            {
                if (!m_storageResource.addFileEntry(m_fileName))
                    return false;
            }
            openMode |= QIODevice::ReadOnly;
        }
        m_file.setFileName(m_storageResource.getUrl());
        if (!m_file.open(openMode))
            return false;
        m_fileOffset = m_storageResource.getFileOffset(m_fileName, &m_fileSize);
        if (m_fileOffset == -1)
            return false;
        bool rez = QIODevice::open(openMode);
        seek(0);
        return rez;
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
    mutable QnMutex m_mutex;
    QnLayoutFileStorageResource& m_storageResource;

    qint64 m_fileOffset;
    qint64 m_fileSize;
    QString m_fileName;
    qint64 m_storedPosition;
    QIODevice::OpenMode m_openMode;
};

// -------------------------------- QnLayoutFileStorageResource ---------------------------

QnMutex QnLayoutFileStorageResource::m_storageSync;
QSet<QnLayoutFileStorageResource*> QnLayoutFileStorageResource::m_allStorages;

QIODevice* QnLayoutFileStorageResource::open(const QString& url, QIODevice::OpenMode openMode)
{
    if (getUrl().isEmpty())
    {
        QString layoutUrl = url;
        NX_ASSERT(url.startsWith(kLayoutProtocol));
        if (layoutUrl.startsWith(kLayoutProtocol))
            layoutUrl = layoutUrl.mid(kLayoutProtocol.length());

        int postfixPos = layoutUrl.indexOf(L'?');
        if (postfixPos == -1)
            setUrl(layoutUrl);
        else
            setUrl(layoutUrl.left(postfixPos));
    }

#ifdef _DEBUG
    if (openMode & QIODevice::WriteOnly)
    {
        for (QSet<QnLayoutFile*>::iterator itr = m_openedFiles.begin(); itr != m_openedFiles.end(); ++itr)
        {
            QnLayoutFile* storage = *itr;
            if (storage->openMode() & QIODevice::WriteOnly)
                NX_ASSERT(false, Q_FUNC_INFO, "Can't open several files for writing");
        }
    }
#endif

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
    QnMutexLocker lock( &m_fileSync );
    m_openedFiles.insert(file);
}

void QnLayoutFileStorageResource::unregisterFile(QnLayoutFile* file)
{
    QnMutexLocker lock( &m_fileSync );
    m_openedFiles.remove(file);
}

void QnLayoutFileStorageResource::closeOpenedFiles()
{
    QnMutexLocker lock( &m_fileSync );
    for (QSet<QnLayoutFile*>::Iterator itr = m_openedFiles.begin(); itr != m_openedFiles.end(); ++itr)
    {
        QnLayoutFile* file = *itr;
        file->lockFile();
        file->storeStateAndClose();
        file->unlockFile();
    }
    m_index.entryCount = 0;
}

void QnLayoutFileStorageResource::restoreOpenedFiles()
{
    QnMutexLocker lock( &m_fileSync );
    for (QSet<QnLayoutFile*>::Iterator itr = m_openedFiles.begin(); itr != m_openedFiles.end(); ++itr)
    {
        QnLayoutFile* file = *itr;
        file->lockFile();
        file->restoreState();
        file->unlockFile();
    }
}

QnLayoutFileStorageResource::QnLayoutFileStorageResource(QnCommonModule* commonModule):
    base_type(commonModule),
    m_fileSync(QnMutex::Recursive),
    m_capabilities(0)
{
    QnMutexLocker lock(&m_storageSync);
    m_novFileOffset = 0;
    //m_novFileLen = 0;
    m_allStorages.insert(this);

    m_capabilities |= cap::ListFile;
    m_capabilities |= cap::ReadFile;
}

QnLayoutFileStorageResource::~QnLayoutFileStorageResource()
{
    QnMutexLocker lock( &m_storageSync );
    m_allStorages.remove(this);
}

bool QnLayoutFileStorageResource::removeFile(const QString& url)
{
    return QFile::remove(url);
}

bool QnLayoutFileStorageResource::renameFile(const QString& oldName, const QString& newName)
{
    return switchToFile(oldName, newName, true);
}

bool QnLayoutFileStorageResource::switchToFile(const QString& oldName, const QString& newName, bool dataInOldFile)
{
    QnMutexLocker lock( &m_storageSync );
    for (QSet<QnLayoutFileStorageResource*>::Iterator itr = m_allStorages.begin(); itr != m_allStorages.end(); ++itr)
    {
        QnLayoutFileStorageResource* storage = *itr;
        QString storageUrl = storage->getPath();
        if (storageUrl == newName || storageUrl == oldName)
            storage->closeOpenedFiles();
    }

    bool rez = true;
    if (dataInOldFile)
    {
        QFile::remove(newName);
        rez = QFile::rename(oldName, newName);
    }
    else
    {
        QFile::remove(oldName);
    }

    for (QSet<QnLayoutFileStorageResource*>::Iterator itr = m_allStorages.begin(); itr != m_allStorages.end(); ++itr)
    {
        QnLayoutFileStorageResource* storage = *itr;
        QString storageUrl = storage->getPath();
        if (storageUrl == newName)
        {
            storage->setUrl(newName); // update binary offsetvalue
            storage->restoreOpenedFiles();
        }
        else if (storageUrl == oldName)
        {
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
    QDir dir(url);
    QList<QFileInfo> list = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for(const QFileInfo& fi: list)
        removeFile(fi.absoluteFilePath());
    return true;
}

bool QnLayoutFileStorageResource::isDirExists(const QString& url)
{
    QDir d(url);
    return d.exists(url);
}

int QnLayoutFileStorageResource::getCapabilities() const
{
    return m_capabilities;
}

bool QnLayoutFileStorageResource::isFileExists(const QString& url)
{
    return QFile::exists(url);
}

qint64 QnLayoutFileStorageResource::getFreeSpace()
{
    return getDiskFreeSpace(getUrl());
}

qint64 QnLayoutFileStorageResource::getTotalSpace() const
{
    return getDiskTotalSpace(getUrl());
}

QnAbstractStorageResource::FileInfoList QnLayoutFileStorageResource::getFileList(const QString& dirName)
{
    QDir dir;
    dir.cd(dirName);
    return QnAbstractStorageResource::FIListFromQFIList(dir.entryInfoList(QDir::Files));
}

qint64 QnLayoutFileStorageResource::getFileSize(const QString& url) const
{
    Q_UNUSED(url)
    return 0; // not implemented
}

Qn::StorageInitResult QnLayoutFileStorageResource::initOrUpdate()
{
    QString tmpDir = closeDirPath(getPath()) + QLatin1String("tmp")
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

QnStorageResource* QnLayoutFileStorageResource::instance(QnCommonModule* commonModule, const QString&)
{
    return new QnLayoutFileStorageResource(commonModule);
}

bool QnLayoutFileStorageResource::readIndexHeader()
{
    QFile file(getUrl());
    if (!file.open(QIODevice::ReadOnly))
        return false;

    file.seek(m_novFileOffset);
    file.read((char*) &m_index, sizeof(m_index));
    if ((quint64)m_index.magic != MAGIC_STATIC) {
        qWarning() << "Invalid nov index detected! Disk write error or antivirus activity. Ignoring.";
        m_index = QnLayoutFileIndex();
        return false;
    }

    if (m_index.entryCount > kMaxEntries) {
        qWarning() << "Corrupted nov file. Ignoring.";
        m_index = QnLayoutFileIndex();
        return false;
    }

    if (m_index.version > kMaxVersion) {
        qWarning() << "Unsupported file from the future version. Ignoring.";
        m_index = QnLayoutFileIndex();
        return false;
    }

    return true;
}

int QnLayoutFileStorageResource::getPostfixSize() const
{
    return m_novFileOffset == 0 ? 0 : sizeof(qint64)*2;
}

bool QnLayoutFileStorageResource::addFileEntry(const QString& srcFileName)
{
    QnMutexLocker lock( &m_fileSync );

    QString fileName = srcFileName.mid(srcFileName.lastIndexOf(L'?')+1);

    QFile file(getUrl());
    qint64 fileSize = file.size() -  getPostfixSize();
    if (fileSize > 0)
        readIndexHeader();
    else
        fileSize = sizeof(m_index);

    if (m_index.entryCount >= (quint32)MAX_FILES_AT_LAYOUT)
        return false;

#ifdef _DEBUG
    qint64 testPos = 0;
    NX_ASSERT(getFileOffset(srcFileName, &testPos) == -1, Q_FUNC_INFO, "Duplicate file name");
#endif

    m_index.entries[m_index.entryCount++] = QnLayoutFileIndexEntry(fileSize - m_novFileOffset, qt4Hash(fileName));

    if (!file.open(QIODevice::ReadWrite))
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
    QnMutexLocker lock( &m_fileSync );

    *fileSize = 0;
    if (m_index.entryCount == 0)
        readIndexHeader();

    QFile file(getUrl());
    if (!file.open(QIODevice::ReadOnly))
        return -1;

    quint32 hash = qt4Hash(fileName);
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
    NX_ASSERT(!value.startsWith(kLayoutProtocol), "Only file links must have layout protocol.");

    setId(QnUuid::createUuid());
    QnStorageResource::setUrl(value);
    if (value.endsWith(QLatin1String(".exe")) || value.endsWith(QLatin1String(".exe.tmp")))
    {
        // find nov file inside binary
        QFile f(value);
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
            }
        }
    }
    else {
        m_novFileOffset = 0;
    }
}

void QnLayoutFileStorageResource::addBinaryPostfix(QFile& file)
{
    file.write((char*) &m_novFileOffset, sizeof(qint64));

    const quint64 magic = FileTypeSupport::NOV_EXE_MAGIC;
    file.write((char*) &magic, sizeof(qint64));
}

QnTimePeriodList QnLayoutFileStorageResource::getTimePeriods(const QnResourcePtr &resource)
{
    QString url = resource->getUrl();
    url = url.mid(url.lastIndexOf(L'?')+1);
    QIODevice* chunkData = open(QString(QLatin1String("chunk_%1.bin")).arg(QnFile::baseName(url)), QIODevice::ReadOnly);
    if (!chunkData)
        return QnTimePeriodList();
    QnTimePeriodList chunks;
    QByteArray chunkDataArray(chunkData->readAll());
    chunks.decode(chunkDataArray);
    delete chunkData;

    return chunks;
}

QString QnLayoutFileStorageResource::itemUniqueId(const QString& layoutUrl,
    const QString& itemUniqueId)
{
    static const QChar kDelimiter = L'?';

    QString itemIdInternal = itemUniqueId.mid(itemUniqueId.lastIndexOf(kDelimiter) + 1);
    QString layoutUrlInternal = layoutUrl;
    NX_ASSERT(!layoutUrlInternal.startsWith(kLayoutProtocol));
    return kLayoutProtocol + layoutUrlInternal + kDelimiter + itemIdInternal;
}

QString QnLayoutFileStorageResource::getPath() const
{
    return getUrl();
}
