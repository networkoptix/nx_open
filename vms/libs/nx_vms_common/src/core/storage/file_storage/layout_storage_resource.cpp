// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_storage_resource.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>

#include <core/resource/avi/filetypesupport.h>
#include <nx/core/layout/layout_file_info.h>
#include <nx/utils/crypt/crypto_functions.h>
#include <nx/utils/fs/file.h>
#include <nx/utils/random.h>
#include <recording/time_period_list.h>
#include <utils/common/util.h>

#include "layout_storage_cryptostream.h"
#include "layout_storage_filestream.h"

namespace {
    /* Max future nov file version that should be opened by the current client version. */
    const qint32 kMaxVersion = 1024;

    /* Protocol for items on the exported layouts. */
    static const QString kLayoutProtocol("layout://");
} // namespace

using namespace nx::core::layout;
using namespace nx::crypt;

struct QnLayoutFileStorageResourceGlobalContext
{
    nx::Mutex storageSync;
    QSet<QnLayoutFileStorageResource*> allStorages;

    static QnLayoutFileStorageResourceGlobalContext& instance()
    {
        static QnLayoutFileStorageResourceGlobalContext inst;
        return inst;
    }
};

QnLayoutFileStorageResource::QnLayoutFileStorageResource():
    base_type(),
    m_fileSync(nx::Mutex::Recursive)
{
    NX_MUTEX_LOCKER lock(&QnLayoutFileStorageResourceGlobalContext::instance().storageSync);
    QnLayoutFileStorageResourceGlobalContext::instance().allStorages.insert(this);
}

QnLayoutFileStorageResource::QnLayoutFileStorageResource(const QString& url):
    QnLayoutFileStorageResource()
{
    setUrl(getFileName(url));
}

QnLayoutFileStorageResource::~QnLayoutFileStorageResource()
{
    NX_MUTEX_LOCKER lock(&QnLayoutFileStorageResourceGlobalContext::instance().storageSync);
    QnLayoutFileStorageResourceGlobalContext::instance().allStorages.remove(this);
}

QnStorageResource* QnLayoutFileStorageResource::instance(const QString& url)
{
    return new QnLayoutFileStorageResource(url);
}

bool QnLayoutFileStorageResource::isEncrypted() const
{
    return m_info.isCrypted;
}

bool QnLayoutFileStorageResource::usePasswordToRead(const QString& password)
{
    if (m_info.isCrypted && checkPassword(password, m_info))
    {
        m_password = password;
        return true;
    }
    return false;
}

void QnLayoutFileStorageResource::setPasswordToWrite(const QString& password)
{
    NX_ASSERT(!password.isEmpty());
    NX_ASSERT(m_index.entryCount == 0, "Set password _before_ export");
    m_info.isCrypted = true;
    m_index.magic = kIndexCryptedMagic;
    m_password = password;

    // Create new salt and salted password hash.
    m_cryptoInfo.passwordSalt = getRandomSalt();
    m_cryptoInfo.passwordHash = getSaltedPasswordHash(m_password, m_cryptoInfo.passwordSalt);
}

void QnLayoutFileStorageResource::forgetPassword()
{
    closeOpenedFiles(); //< Also saves state, but it is negligible.
    m_password = QString();
}

QString QnLayoutFileStorageResource::password() const
{
    return m_password;
}

void QnLayoutFileStorageResource::setUrl(const QString& value)
{
    NX_ASSERT(!value.startsWith(kLayoutProtocol), "Only file links must have layout protocol.");

    setIdUnsafe(nx::Uuid::createUuid());
    QnStorageResource::setUrl(value);

    readIndexHeader();
}

QIODevice* QnLayoutFileStorageResource::openInternal(const QString& url, QIODevice::OpenMode openMode)
{
    NX_MUTEX_LOCKER lock(&m_fileSync);

    if (m_lockedOpenings) //< This is used when renaming or removing layout files.
        return nullptr;

    // Set Url if it does not exist yet.
    if (getUrl().isEmpty())
    {
        NX_ASSERT(false, "Now url should be set before open."); //< Also we may lose password if we get here.
        NX_ASSERT(url.startsWith(kLayoutProtocol));
        setUrl(getFileName(url));
    }

#ifdef _DEBUG
    if (openMode & QIODevice::WriteOnly)
    {
        for (auto storage : m_openedFiles)
        {
            // Enjoy cross cast - it actually works!
            if (dynamic_cast<QIODevice*>(storage)->openMode() & QIODevice::WriteOnly)
                NX_ASSERT(false, "Can't open several files for writing");
        }
    }
#endif

    // Can only open valid file for reading.
    if (!(openMode & QIODevice::WriteOnly) && !m_info.isValid)
        return nullptr;

    const auto fileName = stripName(url);

    QScopedPointer<QIODevice> stream;
    if (shouldCrypt(fileName))
    {
        NX_ASSERT(!(openMode & QIODevice::WriteOnly) || !m_password.isEmpty()); // Want to write but no password.
        if (m_password.isEmpty()) // Cannot read crypted stream without a password.
            return nullptr;

        stream.reset(new QnLayoutCryptoStream(*this, url, m_password));
    }
    else
        stream.reset(new QnLayoutPlainStream(*this, url));

    if (!stream->open(openMode))
        return nullptr;

    return stream.take();
}

int QnLayoutFileStorageResource::getCapabilities() const
{
    return ListFile | ReadFile;
}

nx::vms::api::StorageInitResult QnLayoutFileStorageResource::initOrUpdate()
{
     const QString tmpDir = closeDirPath(getUrl())
        + "tmp"
        + QString::number(nx::utils::random::number<uint>());

    QDir dir(tmpDir);
    if (dir.exists() && dir.removeRecursively())
        return nx::vms::api::StorageInitResult::ok;

    if (dir.mkpath(tmpDir) && dir.rmdir(tmpDir))
        return nx::vms::api::StorageInitResult::ok;

    return nx::vms::api::StorageInitResult::wrongPath;
}

QnAbstractStorageResource::FileInfoList QnLayoutFileStorageResource::getFileList(
    const QString& dirName)
{
    QDir dir;
    if (dir.cd(dirName))
        return FIListFromQFIList(dir.entryInfoList(QDir::Files));

    return FileInfoList();
}

qint64 QnLayoutFileStorageResource::getFileSize(const QString& /*url*/) const
{
    NX_ASSERT("false", "This is a server-only method");
    return 0;
}

bool QnLayoutFileStorageResource::removeFile(const QString& url)
{
    return QFile::remove(url);
}

bool QnLayoutFileStorageResource::removeDir(const QString& url)
{
    QDir dir(url);
    QList<QFileInfo> list = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for(const QFileInfo& fi: list)
        removeFile(fi.absoluteFilePath());
    return true;
}

bool QnLayoutFileStorageResource::renameFile(const QString& oldName, const QString& newName)
{
    return switchToFile(oldName, newName, true);
}

bool QnLayoutFileStorageResource::isFileExists(const QString& url)
{
    return QFile::exists(getFileName(url));
}

bool QnLayoutFileStorageResource::isDirExists(const QString& url)
{
    QDir d(url);
    return d.exists(url);
}

qint64 QnLayoutFileStorageResource::getFreeSpace()
{
    NX_ASSERT("false", "This is a server-only method");
    return 0;
}

qint64 QnLayoutFileStorageResource::getTotalSpace() const
{
    NX_ASSERT("false", "This is a server-only method");
    return 0;
}

bool QnLayoutFileStorageResource::switchToFile(const QString& oldName, const QString& newName, bool dataInOldFile)
{
    NX_MUTEX_LOCKER lock(&QnLayoutFileStorageResourceGlobalContext::instance().storageSync);
    for (auto storage: QnLayoutFileStorageResourceGlobalContext::instance().allStorages)
    {
        QString storageUrl = storage->getUrl();
        if (storageUrl == newName || storageUrl == oldName)
        {
            storage->lockOpenings();
            storage->closeOpenedFiles();
        }
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

    for (auto storage: QnLayoutFileStorageResourceGlobalContext::instance().allStorages)
    {
        QString storageUrl = storage->getUrl();
        if (storageUrl == newName || storageUrl == oldName)
        {
            storage->setUrl(newName); // update binary offset value
            storage->unlockOpenings();
            storage->restoreOpenedFiles();
        }
    }
    if (rez)
        setUrl(newName);
    return rez;
}

QnTimePeriodList QnLayoutFileStorageResource::getTimePeriods(const QnResourcePtr &resource)
{
    QString url = resource->getUrl();
    url = url.mid(url.lastIndexOf('?')+1);
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
    static const QChar kDelimiter = '?';

    QString itemIdInternal = itemUniqueId.mid(itemUniqueId.lastIndexOf(kDelimiter) + 1);
    QString layoutUrlInternal = layoutUrl;
    NX_ASSERT(!layoutUrlInternal.startsWith(kLayoutProtocol));
    return kLayoutProtocol + layoutUrlInternal + kDelimiter + itemIdInternal;
}

// This function assumes index is already read by setUrl() (maybe called from open() ).
QnLayoutFileStorageResource::Stream QnLayoutFileStorageResource::findStream(const QString& name)
{
    NX_MUTEX_LOCKER lock(&m_fileSync);

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
                    qint64 endPos = file.size() - getTailSize();
                    stream.size = endPos - stream.position;
                }
                return stream;
            }
        }
    }
    return Stream();
}

// This function may create a new .nov file.
QnLayoutFileStorageResource::Stream QnLayoutFileStorageResource::findOrAddStream(const QString& name)
{
    NX_MUTEX_LOCKER lock(&m_fileSync);

    QString fileName = stripName(name);

    const auto stream = findStream(name);
    if (stream)
        return stream;

    // At this point m_info and m_index are read in findStream if existed.
    // Exe: the file is not valid (m_info.isValid) at this point. Will write a new index a few lines later.

    if (m_index.entryCount >= kMaxStreams)
        return Stream();

    if (!m_info.isValid) //< Could not access the file or it is empty.
        if (!writeIndexHeader())
            return Stream();

    QFile file(getUrl());
    const qint64 fileSize = file.size() -  getTailSize();

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
    writeFileTail(file);

    return Stream{fileSize + utf8FileName.size() + 1, 0};
}

void QnLayoutFileStorageResource::finalizeWrittenStream(qint64 pos)
{
    if (m_info.offset > 0)
    {
        QFile file(getUrl());
        file.open(QIODevice::Append);
        file.seek(pos);
        writeFileTail(file);
    }
}

void QnLayoutFileStorageResource::registerFile(QnLayoutStreamSupport* file)
{
    NX_MUTEX_LOCKER lock(&m_fileSync);
    m_openedFiles.insert(file);
}

void QnLayoutFileStorageResource::unregisterFile(QnLayoutStreamSupport* file)
{
    NX_MUTEX_LOCKER lock(&m_fileSync);
    m_openedFiles.remove(file);
}

void QnLayoutFileStorageResource::removeFileCompletely(QnLayoutStreamSupport* file)
{
    NX_MUTEX_LOCKER lock(&m_fileSync);
    m_openedFiles.remove(file);
    m_cachedOpenedFiles.remove(file);
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

bool QnLayoutFileStorageResource::writeIndexHeader()
{
    QFile file(getUrl());
    // This function _appends_ header if we write to exe file.
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
        return false;

    m_info.offset = file.pos();

    file.write((const char*)&m_index, sizeof(m_index));
    if (m_info.isCrypted)
        file.write((const char*)&m_cryptoInfo, sizeof(CryptoInfo));

    if (file.error() != QFile::NoError)
        return false;

    writeFileTail(file);

    m_info.isValid = true;
    return true;
}

int QnLayoutFileStorageResource::getTailSize() const
{
    return m_info.offset == 0 ? 0 : sizeof(qint64) * 2;
}

void QnLayoutFileStorageResource::writeFileTail(QFile& file)
{
    if (m_info.offset > 0)
    {
        file.write((char*)&m_info.offset, sizeof(qint64));
        const quint64 magic = nx::core::layout::kFileMagic;
        file.write((char*)&magic, sizeof(qint64));
    }
}

void QnLayoutFileStorageResource::closeOpenedFiles()
{
    NX_MUTEX_LOCKER lock(&m_fileSync);
    m_cachedOpenedFiles = m_openedFiles; // m_openedFiles will be cleared in storeStateAndClose().
    for (auto file: m_cachedOpenedFiles)
        file->storeStateAndClose();
}

void QnLayoutFileStorageResource::restoreOpenedFiles()
{
    NX_MUTEX_LOCKER lock(&m_fileSync);
    for (auto file: m_cachedOpenedFiles)
        file->restoreState();
}

bool QnLayoutFileStorageResource::shouldCrypt(const QString& streamName)
{
    if (!isEncrypted())
        return false;

    return FileTypeSupport::isMovieFileExt(streamName)
        || FileTypeSupport::isImageFileExt(streamName);
}

QString QnLayoutFileStorageResource::stripName(const QString& fileName)
{
    return fileName.mid(fileName.lastIndexOf('?') + 1);
}

QString QnLayoutFileStorageResource::getFileName(const QString& url)
{
    return url.left(url.indexOf('?')).remove(kLayoutProtocol);
}

void QnLayoutFileStorageResource::lockOpenings()
{
    NX_MUTEX_LOCKER lock(&m_fileSync);
    m_lockedOpenings = true;
}

void QnLayoutFileStorageResource::unlockOpenings()
{
    NX_MUTEX_LOCKER lock(&m_fileSync);
    m_lockedOpenings = false;
}

nx::Mutex& QnLayoutFileStorageResource::streamMutex()
{
    return m_fileSync;
}

// The one who requests to remove this function will become a permanent maintainer of this class.
void QnLayoutFileStorageResource::dumpStructure()
{
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
            << "size:" << Qt::hex << m_index.entries[i + 1].offset - m_index.entries[i].offset
            << "adjusted:" << Qt::hex << m_index.entries[i + 1].offset - m_index.entries[i].offset -
            strlen(actualFileName.data()) - 1;
    }
}
