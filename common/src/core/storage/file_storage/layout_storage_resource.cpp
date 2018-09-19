#include "layout_storage_resource.h"
#include "layout_storage_filestream.h"
#include "layout_storage_cryptostream.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>

#include <nx/core/layout/layout_file_info.h>
#include <nx/utils/random.h>

#include <recording/time_period_list.h>
#include <utils/common/util.h>
#include <utils/fs/file.h>

namespace {
    /* Max future nov-file version that should be opened by the current client version. */
    const qint32 kMaxVersion = 1024;

    /* Protocol for items on the exported layouts. */
    static const QString kLayoutProtocol(lit("layout://"));
} // namespace

using namespace nx::core::layout;

QnMutex QnLayoutFileStorageResource::m_storageSync;
QSet<QnLayoutFileStorageResource*> QnLayoutFileStorageResource::m_allStorages;

QIODevice* QnLayoutFileStorageResource::open(const QString& url, QIODevice::OpenMode openMode)
{
    qDebug() << "QnLayoutFileStorageResource::open" << getUrl() << url;
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
        for (auto storage : m_openedFiles)
        {
            // Enjoy cross cast - it actually works!
            if (dynamic_cast<QIODevice*>(storage)->openMode() & QIODevice::WriteOnly)
                NX_ASSERT(false, Q_FUNC_INFO, "Can't open several files for writing");
        }
    }
#endif

    // Can only open valid file for reading.
    if (!(openMode & QIODevice::WriteOnly) && !m_info.isValid)
        return nullptr;

    QIODevice* rez = new QnLayoutPlainStream(*this, url);
//    QIODevice* rez = new QnLayoutCryptoStream(*this, url, "helloworld");
    if (!rez->open(openMode))
    {
        delete rez;
        return nullptr;
    }
    return rez;
}

void QnLayoutFileStorageResource::registerFile(QnLayoutStreamSupport* file)
{
    QnMutexLocker lock( &m_fileSync );
    m_openedFiles.insert(file);
}

void QnLayoutFileStorageResource::unregisterFile(QnLayoutStreamSupport* file)
{
    QnMutexLocker lock( &m_fileSync );
    m_openedFiles.remove(file);
}

void QnLayoutFileStorageResource::closeOpenedFiles()
{
    QnMutexLocker lock( &m_fileSync );
    m_cachedOpenedFiles = m_openedFiles; // m_openedFiles will be cleared in storeStateAndClose().
    for (auto file : m_cachedOpenedFiles)
    {
        file->lockFile();
        file->storeStateAndClose();
        file->unlockFile();
    }
    m_index.entryCount = 0;
}

void QnLayoutFileStorageResource::restoreOpenedFiles()
{
    QnMutexLocker lock( &m_fileSync );
    for (auto file : m_cachedOpenedFiles)
    {
        file->lockFile();
        file->restoreState();
        file->unlockFile();
    }
}

QnLayoutFileStorageResource::QnLayoutFileStorageResource(QnCommonModule* commonModule):
    base_type(commonModule),
    m_fileSync(QnMutex::Recursive)
{
    qDebug() << "QnLayoutFileStorageResource::QnLayoutFileStorageResource" << getUrl();
    QnMutexLocker lock(&m_storageSync);
    m_allStorages.insert(this);
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
    for (auto itr = m_allStorages.begin(); itr != m_allStorages.end(); ++itr)
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

    for (auto itr = m_allStorages.begin(); itr != m_allStorages.end(); ++itr)
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
    return ListFile | ReadFile;
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

qint64 QnLayoutFileStorageResource::getFileSize(const QString& /*url*/) const
{
    return 0; //< not implemented
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

bool QnLayoutFileStorageResource::isCrypted() const
{
    return m_info.isCrypted;
}

void QnLayoutFileStorageResource::usePasswordForExport(const QString& password)
{
    // TODO
}

bool QnLayoutFileStorageResource::usePasswordToOpen(const QString& password)
{
    if (!m_info.isCrypted)
        return false;
    return true;
    // TODO
//    if (checkPassword()
}

bool QnLayoutFileStorageResource::readIndexHeader()
{
    m_info = identifyFile(getUrl(), true);
    if (!m_info.isValid)
    {
        qWarning() << "Nonexistent or corrupted nov file. Ignoring.";
        return false;
    }

    QFile file(getUrl());
    if (!file.open(QIODevice::ReadOnly))
        return false;

    file.seek(m_info.offset);
    file.read((char*)&m_index, sizeof(m_index));

    if (m_info.isCrypted)
        file.read((char*) &m_cryptoInfo, sizeof(m_cryptoInfo));
    if (m_index.entryCount > kMaxStreams) //< There was kMaxFiles == 1024, but StreamIndex has only 256 entries.
    {
        qWarning() << "Corrupted nov file. Ignoring.";
        m_index = StreamIndex();
        return false;
    }

    if (m_info.version > kMaxVersion)
    {
        qWarning() << "Unsupported file from the future version. Ignoring.";
        m_index = StreamIndex();
        return false;
    }

    return true;
}

bool QnLayoutFileStorageResource::createNewFile()
{
    QFile file(getUrl());
    if (!file.open(QIODevice::WriteOnly))
        return false;

    file.write((const char*)&m_index, sizeof(m_index));

    if (m_info.isCrypted)
        file.write((const char*)&m_cryptoInfo, sizeof(CryptoInfo));

    if (file.error() != QFile::NoError)
        return false;

    m_info.isValid = true;
    return true;
}

int QnLayoutFileStorageResource::getPostfixSize() const
{
    return m_info.offset == 0 ? 0 : sizeof(qint64) * 2;
}

QString QnLayoutFileStorageResource::stripName(const QString& fileName)
{
    return fileName.mid(fileName.lastIndexOf(L'?') + 1);
}

// This function may create a new .nov file.
QnLayoutFileStorageResource::Stream QnLayoutFileStorageResource::findOrAddStream(const QString& name)
{
    QnMutexLocker lock( &m_fileSync );

    QString fileName = stripName(name);

    const auto stream = findStream(name);
    if (stream)
        return stream;

    // At this point m_info and m_index are read in findStream if existed.
    // It is the same for exe, because index is already written by QnNovLauncher::createLaunchingFile.

    if (m_index.entryCount >= kMaxStreams)
        return Stream();

    if (!m_info.isValid) //< Could not access the file or it is empty.
        if (!createNewFile())
            return Stream();

    QFile file(getUrl());
    const qint64 fileSize = file.size() -  getPostfixSize();

    m_index.entries[m_index.entryCount++] =
        StreamIndexEntry{fileSize - m_info.offset, qt4Hash(fileName)};

    if (!file.open(QIODevice::ReadWrite))
        return Stream();
    // Write new or updated index.
    file.seek(m_info.offset);
    file.write((const char*) &m_index, sizeof(m_index));
    file.seek(fileSize);

    // Write new stream name.
    QByteArray utf8FileName = fileName.toUtf8();
    utf8FileName.append('\0');
    file.write(utf8FileName);

    // Add ending magic string.
    if (m_info.offset > 0)
        addBinaryPostfix(file);

    return Stream{fileSize + utf8FileName.size() + 1, 0};
}

// This function assumes index is already read by setUrl() (maybe called from open() ).
QnLayoutFileStorageResource::Stream QnLayoutFileStorageResource::findStream(const QString& name)
{
    QnMutexLocker lock(&m_fileSync);

    if (m_index.entryCount == 0)
        return Stream();

    QFile file(getUrl());
    if (!file.open(QIODevice::ReadOnly))
        return Stream();

    const QString fileName = stripName(name);
    quint32 hash = qt4Hash(fileName);
    QByteArray utf8FileName = fileName.toUtf8();
    for (uint i = 0; i < m_index.entryCount; ++i)
    {
        if (m_index.entries[i].fileNameCrc == hash)
        {
            file.seek(m_index.entries[i].offset + m_info.offset);
            char tmpBuffer[1024]; // buffer size is max file len
            int readBytes = file.read(tmpBuffer, sizeof(tmpBuffer));
            QByteArray actualFileName(tmpBuffer, qMin(readBytes, utf8FileName.length()));
            if (utf8FileName == actualFileName)
            {
                Stream stream;
                stream.position = m_index.entries[i].offset + m_info.offset
                    + fileName.toUtf8().length() + 1;
                if (i < m_index.entryCount-1)
                    stream.size = m_index.entries[i + 1].offset + m_info.offset - stream.position;
                else
                {
                    qint64 endPos = file.size() - getPostfixSize();
                    stream.size = endPos - stream.position;
                }
                return stream;
            }
        }
    }
    return Stream();
}

void QnLayoutFileStorageResource::finalizeWrittenStream(qint64 pos)
{
    if (m_info.offset > 0)
    {
        QFile file(getUrl());
        file.open(QIODevice::Append);
        file.seek(pos);
        addBinaryPostfix(file);
    }
}

void QnLayoutFileStorageResource::setUrl(const QString& value)
{
    qDebug() << "QnLayoutFileStorageResource::setUrl" << value;
    NX_ASSERT(!value.startsWith(kLayoutProtocol), "Only file links must have layout protocol.");

    setId(QnUuid::createUuid());
    QnStorageResource::setUrl(value);

    readIndexHeader();
}

void QnLayoutFileStorageResource::addBinaryPostfix(QFile& file)
{
    file.write((char*) &m_info.offset, sizeof(qint64));

    const quint64 magic = nx::core::layout::kFileMagic;
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

// The one who requests to remove this function will become a permanent maintainer of this class.
void QnLayoutFileStorageResource::dumpStructure()
{
    if (!getUrl().contains(".exe"))
        return;

    qDebug() << "Logging" << getUrl();
    QFile file(getUrl());
    file.open(QIODevice::ReadOnly);

    if (m_index.entryCount == 0)
        return;

    for(quint32 i = 0; i < m_index.entryCount - 1; i++)
    {
        char tmpBuffer[1024]; // buffer size is max file len
        file.seek(m_index.entries[i].offset + m_info.offset);
        const int readBytes = file.read(tmpBuffer, sizeof(tmpBuffer));
        QByteArray actualFileName(tmpBuffer, readBytes);
        qDebug() << "Entry" << i << QString(actualFileName)
            << "size:" << hex <<m_index.entries[i + 1].offset - m_index.entries[i].offset
            << "adjusted:" << hex << m_index.entries[i + 1].offset - m_index.entries[i].offset -
            strlen(actualFileName.data()) - 1;
    }
}
