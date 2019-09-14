#include <stdexcept>
#include <memory>
#include "third_party_storage_resource.h"
#include "media_server/settings.h"
#include "utils/common/buffered_file.h"
#include "utils/fs/file.h"
#include <media_server/media_server_module.h>

using namespace nx::sdk;

namespace aux {

class ThirdPartyIODevice: public QIODevice
{
public:
    ThirdPartyIODevice(const Ptr<nx_spl::IODevice>& io, OpenMode mode): m_io(io)
    {
        if (!m_io)
            throw std::runtime_error("IODevice is NULL");
        open(mode);
    }

    ~ThirdPartyIODevice()
    {
        QIODevice::close();
    }

public: // overriding IODevice functions
    qint64 readData(char* data, qint64 maxLen) override
    {
        if (!m_io)
           return -1;

        int errorCode;
        const uint32_t bytesRead = m_io->read(data, maxLen, &errorCode);

        if (errorCode != nx_spl::error::NoError)
            return -1;

        return bytesRead;
    }

    qint64 writeData(const char *data, qint64 len) override
    {
        if (!m_io)
            return -1;

        int errorCode;
        m_io->write(data, len, &errorCode);

        if (errorCode != nx_spl::error::NoError)
            return -1;

        return len;
    }

    qint64 size() const override
    {
        int errorCode;
        const qint64 fileSize = m_io->size(&errorCode);
        if (errorCode != nx_spl::error::NoError)
            return -1;
        return fileSize;
    }

    bool seek(qint64 pos) override
    {
        if (!QIODevice::seek(pos))
            return false;
        int errorCode;
        const int result = m_io->seek(pos, &errorCode);
        if (result == 0 || errorCode != nx_spl::error::NoError)
            return false;
        return true;
    }

    void close() override
    {
        QIODevice::close();
        m_io.reset();
    }

private:
    Ptr<nx_spl::IODevice> m_io;
};

class ThirdPartyFile: public IQnFile
{
public:
    ThirdPartyFile(const QString &filename, std::shared_ptr<ThirdPartyIODevice> impl):
        m_filename(filename),
        m_impl(std::move(impl))
    {
    }

    virtual QString getFileName() const override
    {
        return m_filename;
    }

    virtual bool open(
        const QIODevice::OpenMode& mode, unsigned int /*systemDependentFlags*/) override
    {
        return m_impl->open(mode);
    }

    virtual void close() override
    {
        m_impl->close();
    }

    virtual qint64 read(char* buffer, qint64 count) override
    {
        return m_impl->read(buffer, count);
    }

    virtual qint64 write(const char* buffer, qint64 count) override
    {
        return m_impl->write(buffer, count);
    }

    virtual bool isOpen() const override
    {
        return m_impl->isOpen();
    }

    virtual qint64 size() const override
    {
        return m_impl->size();
    }

    virtual bool seek(qint64 offset) override
    {
        return m_impl->seek(offset);
    }

    virtual bool truncate(qint64 /*newFileSize*/) override
    {
        // Generally, third-party storages aren't supposed to support truncating.
        NX_ASSERT(false);
        return false;
    }

    virtual bool eof() const override
    {
        return m_impl->atEnd();
    }

private:
    QString m_filename;
    std::shared_ptr<ThirdPartyIODevice> m_impl;
};

} //namespace aux

QnStorageResource* QnThirdPartyStorageResource::instance(
    QnCommonModule* commonModule,
    const QString& url,
    nx_spl::StorageFactory* sf,
    const nx::vms::server::Settings* settings)
{
    try
    {
        return new QnThirdPartyStorageResource(commonModule, sf, url, settings);
    }
    catch (...)
    {
        return nullptr;
    }
}

QnThirdPartyStorageResource::QnThirdPartyStorageResource(
    QnCommonModule* commonModule,
    nx_spl::StorageFactory* sf,
    const QString& storageUrl,
    const nx::vms::server::Settings* settings)
    :
    QnStorageResource(commonModule),
    m_valid(true),
    m_settings(settings)
{
    openStorage(storageUrl.toLatin1().constData(), sf);
}

void QnThirdPartyStorageResource::openStorage(
    const char* storageUrl, nx_spl::StorageFactory* storageFactory)
{
    QnMutexLocker lock(&m_mutex);

    int errorCode;
    const auto storage = toPtr(storageFactory->createStorage(storageUrl, &errorCode));

    if (errorCode != nx_spl::error::NoError)
        throw std::runtime_error("Couldn't create Storage");

    if (!storage)
        throw std::runtime_error("Couldn't create Storage");

    if (!queryInterfaceOfOldSdk<nx_spl::Storage>(storage, nx_spl::IID_Storage))
        throw std::logic_error("Unknown storage interface version");

    m_storage = storage;
}

QIODevice *QnThirdPartyStorageResource::openInternal(
    const QString& fileName, QIODevice::OpenMode openMode)
{
    if (!m_valid)
        return nullptr;

    if (fileName.isEmpty())
        return nullptr;

    int errorCode;
    int ioFlags = 0;

    if (openMode & QIODevice::ReadOnly)
        ioFlags |= nx_spl::io::ReadOnly;
    if (openMode & QIODevice::WriteOnly)
        ioFlags |= nx_spl::io::WriteOnly;

    const auto ioDevice = toPtr(m_storage->open(
        QUrl(fileName).path().toLatin1().constData(),
        ioFlags,
        &errorCode));

    if (!ioDevice || errorCode != nx_spl::error::NoError
        || !queryInterfaceOfOldSdk<nx_spl::IODevice>(ioDevice, nx_spl::IID_IODevice))
    {
        return nullptr;
    }

    int ioBlockSize = 0;
    int ffmpegBufferSize = 0;

    const int ffmpegMaxBufferSize = m_settings->maxFfmpegBufferSize();

    if (openMode & QIODevice::WriteOnly)
    {
        ioBlockSize = m_settings->ioBlockSize();
        ffmpegBufferSize = m_settings->ffmpegBufferSize();
    }

    auto thirdPartyFile = std::make_shared<aux::ThirdPartyFile>(
        fileName, std::make_shared<aux::ThirdPartyIODevice>(ioDevice, openMode));

    auto result = std::make_unique<QBufferedFile>(
        std::move(thirdPartyFile),
        ioBlockSize,
        ffmpegBufferSize,
        ffmpegMaxBufferSize,
        getId());

    if (!result->open(openMode))
        return nullptr;

    return result.release();
}

int QnThirdPartyStorageResource::getCapabilities() const
{
    return m_valid ? m_storage->getCapabilities() : 0;
}

qint64 QnThirdPartyStorageResource::getFreeSpace()
{
    if (!m_valid)
        return 0;

    int errorCode;
    const qint64 freeSpace = m_storage->getFreeSpace(&errorCode);
    if (errorCode != nx_spl::error::NoError)
        return -1;
    return freeSpace;
}

qint64 QnThirdPartyStorageResource::getTotalSpace() const
{
    if (!m_valid)
        return 0;

    int errorCode;
    const qint64 totalSpace = m_storage->getTotalSpace(&errorCode);
    if (errorCode != nx_spl::error::NoError)
        return -1;
    return totalSpace;
}

Qn::StorageInitResult QnThirdPartyStorageResource::initOrUpdate()
{
    if (!m_valid)
       return Qn::StorageInit_WrongPath;
    return m_storage->isAvailable() ? Qn::StorageInit_Ok : Qn::StorageInit_WrongPath;
}

bool QnThirdPartyStorageResource::removeFile(const QString& url)
{
    if (!m_valid)
        return false;

    int errorCode;
    m_storage->removeFile(urlToPath(url).toLatin1().constData(), &errorCode);
    return errorCode == nx_spl::error::NoError;
}

bool QnThirdPartyStorageResource::removeDir(const QString& url)
{
    if (!m_valid)
        return false;

    int errorCode;
    m_storage->removeDir(urlToPath(url).toLatin1().constData(), &errorCode);
    return errorCode == nx_spl::error::NoError;
}

bool QnThirdPartyStorageResource::renameFile(const QString& oldName, const QString& newName)
{
    if (!m_valid)
        return false;

    int errorCode;
    m_storage->renameFile(
        urlToPath(oldName).toLatin1().constData(),
        urlToPath(newName).toLatin1().constData(),
        &errorCode);
    return errorCode == nx_spl::error::NoError;
}

QnAbstractStorageResource::FileInfoList QnThirdPartyStorageResource::getFileList(
    const QString& dirName)
{
    if (!m_valid)
        return FileInfoList();

    QnMutexLocker lock(&m_mutex);

    int errorCode;
    const auto fileInfoIterator = toPtr(m_storage->getFileIterator(
        urlToPath(dirName).toLatin1().constData(),
        &errorCode));

    if (!fileInfoIterator
        || errorCode != nx_spl::error::NoError
        || !queryInterfaceOfOldSdk<nx_spl::FileInfoIterator>(
            fileInfoIterator, nx_spl::IID_FileInfoIterator))
    {
        return FileInfoList();
    }

    FileInfoList result;
    for (nx_spl::FileInfo* fileInfo = fileInfoIterator->next(&errorCode);
        fileInfo && errorCode == nx_spl::error::NoError;
        fileInfo = fileInfoIterator->next(&errorCode))
    {
        QString urlString = fileInfo->url;
        if (!urlString.contains("://"))
            urlString = QUrl(dirName).toString(QUrl::RemovePath) + fileInfo->url;

        result.append(FileInfo(
            urlString,
            fileInfo->size,
            fileInfo->type & nx_spl::isDir));
    }
    return result;
}

bool QnThirdPartyStorageResource::isFileExists(const QString& url)
{
    if (!m_valid)
        return false;

    QnMutexLocker lock(&m_mutex);
    int errorCode;
    m_storage->fileExists(urlToPath(url).toLatin1().constData(), &errorCode);
    return errorCode == nx_spl::error::NoError;
}

bool QnThirdPartyStorageResource::isDirExists(const QString& url)
{
    if (!m_valid)
        return false;

    QnMutexLocker lock(&m_mutex);
    int errorCode;
    const bool result = m_storage->dirExists(urlToPath(url).toLatin1().constData(), &errorCode);
    if (errorCode != nx_spl::error::NoError)
        return false;
    return result;
}

qint64 QnThirdPartyStorageResource::getFileSize(const QString& url) const
{
    if (!m_valid)
        return 0;

    QnMutexLocker lock(&m_mutex);
    int errorCode;
    const qint64 fileSize = m_storage->fileSize(urlToPath(url).toLatin1().constData(), &errorCode);

    if (errorCode != nx_spl::error::NoError)
        return -1;
    return fileSize;
}
