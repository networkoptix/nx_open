#include <stdexcept>
#include <memory>
#include "third_party_storage_resource.h"
#include "media_server/settings.h"
#include "utils/common/buffered_file.h"
#include "utils/fs/file.h"
#include <media_server/media_server_module.h>

using namespace nx::sdk;

namespace aux
{
    class ThirdPartyIODevice
        : public QIODevice
    {
    public: //ctor, dtor
        ThirdPartyIODevice(
            const QnThirdPartyStorageResource::IODevicePtrType &io,
            QIODevice::OpenMode                                 mode
        )
            : m_io(io)
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
        qint64 readData(char *data, qint64 maxlen) override
        {
            if (!m_io)
               return -1;

            int ecode;
            uint32_t bytesRead = m_io->read(data, maxlen, &ecode);

            if (ecode != nx_spl::error::NoError)
                return -1;

            return bytesRead;
        }

        qint64 writeData(const char *data, qint64 len) override
        {
            if (!m_io)
                return -1;

            int ecode;
            m_io->write(data, len, &ecode);

            if (ecode != nx_spl::error::NoError)
                return -1;

            return len;
        }

        qint64 size() const override
        {
            int ecode;
            qint64 fsize = m_io->size(&ecode);
            if (ecode != nx_spl::error::NoError)
                return -1;
            return fsize;
        }

        bool seek(qint64 pos) override
        {
            if (!QIODevice::seek(pos))
                return false;
            int ecode;
            int result = m_io->seek(pos, &ecode);
            if (!result || ecode != nx_spl::error::NoError)
                return false;
            return true;
        }

        void close() override
        {
            QIODevice::close();
            m_io.reset();
        }
    private:
        QnThirdPartyStorageResource::IODevicePtrType m_io;
    }; // class ThirdPartyIODevice

    class ThirdPartyFile : public IQnFile
    {
        typedef std::shared_ptr<ThirdPartyIODevice> TPIODevicePtr;
    public:
        ThirdPartyFile(const QString &fname, const TPIODevicePtr impl)
            : m_filename(fname),
              m_impl(impl)
        {}

        virtual QString getFileName() const override
        {
            return m_filename;
        }

        virtual bool open(
            const QIODevice::OpenMode  &mode,
            unsigned int                /*systemDependentFlags*/
        ) override
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

        virtual bool seek( qint64 offset) override
        {
            return m_impl->seek(offset);
        }

        virtual bool truncate( qint64 /*newFileSize*/) override
        {
            // Generally, third-party storages aren't supposed to
            // support truncating.
            NX_ASSERT(false);
            return false;
        }

        virtual bool eof() const override
        {
            return m_impl->atEnd();
        }

    private:
        QString         m_filename;
        TPIODevicePtr   m_impl;
    };
} //namespace aux

QnStorageResource* QnThirdPartyStorageResource::instance(
    QnCommonModule* commonModule,
    const QString& url,
    const StorageFactoryPtrType& sf,
    const nx::vms::server::Settings* settings
)
{
    try
    {
        return new QnThirdPartyStorageResource(commonModule, sf, url, settings);
    }
    catch(...)
    {
        return nullptr;
    }
}

QnThirdPartyStorageResource::QnThirdPartyStorageResource(
    QnCommonModule* commonModule,
    const StorageFactoryPtrType &sf,
    const QString               &storageUrl,
    const nx::vms::server::Settings* settings)
    :
    base_type(commonModule),
    m_valid(true),
    m_settings(settings)
{
    openStorage(storageUrl.toLatin1().constData(), sf);
}

void QnThirdPartyStorageResource::openStorage(
    const char* storageUrl, const StorageFactoryPtrType& storageFactory)
{
    QnMutexLocker lock(&m_mutex);

    int errorCode;
    const auto storage = toPtr(storageFactory->createStorage(storageUrl, &errorCode));

    if (errorCode != nx_spl::error::NoError)
        throw std::runtime_error("Couldn't create Storage");

    if (!storage)
        throw std::runtime_error("Couldn't create Storage");

    if (!queryInterfacePtr<nx_spl::Storage>(storage, nx_spl::IID_Storage))
        throw std::logic_error("Unknown storage interface version");

    m_storage = storage;
}

QIODevice *QnThirdPartyStorageResource::open(
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
        || !queryInterfacePtr<nx_spl::IODevice>(ioDevice, nx_spl::IID_IODevice))
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

    std::unique_ptr<QBufferedFile> result(
        new QBufferedFile(
            std::shared_ptr<IQnFile>(
                new aux::ThirdPartyFile(
                    fileName,
                    std::make_shared<aux::ThirdPartyIODevice>(ioDevice, openMode))),
            ioBlockSize,
            ffmpegBufferSize,
            ffmpegMaxBufferSize,
            getId()));
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

    int ecode;
    qint64 freeSpace = m_storage->getFreeSpace(&ecode);
    if (ecode != nx_spl::error::NoError)
        return -1;
    return freeSpace;
}

qint64 QnThirdPartyStorageResource::getTotalSpace() const
{
    if (!m_valid)
        return 0;

    int ecode;
    qint64 totalSpace = m_storage->getTotalSpace(&ecode);
    if (ecode != nx_spl::error::NoError)
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

    int ecode;
    m_storage->removeFile(
        urlToPath(url).toLatin1().constData(),
        &ecode
    );
    if (ecode != nx_spl::error::NoError)
        return false;
    return true;
}

bool QnThirdPartyStorageResource::removeDir(const QString& url)
{
    if (!m_valid)
        return false;

    int ecode;
    m_storage->removeDir(
        urlToPath(url).toLatin1().constData(),
        &ecode
    );
    if (ecode != nx_spl::error::NoError)
        return false;
    return true;
}

bool QnThirdPartyStorageResource::renameFile(
    const QString   &oldName,
    const QString   &newName
)
{
    if (!m_valid)
        return false;

    int ecode;
    m_storage->renameFile(
        urlToPath(oldName).toLatin1().constData(),
        urlToPath(newName).toLatin1().constData(),
        &ecode
    );
    if (ecode != nx_spl::error::NoError)
        return false;
    return true;
}


QnAbstractStorageResource::FileInfoList
QnThirdPartyStorageResource::getFileList(const QString& dirName)
{
    if (!m_valid)
        return FileInfoList();

    QnMutexLocker lock(&m_mutex);

    int errorCode;
    const auto fileInfoIterator = toPtr(m_storage->getFileIterator(
        urlToPath(dirName).toLatin1().constData(),
        &errorCode));
    if (!fileInfoIterator || errorCode != nx_spl::error::NoError
        || !queryInterfacePtr<nx_spl::FileInfoIterator>(
            fileInfoIterator, nx_spl::IID_FileInfoIterator))
    {
        return FileInfoList();
    }

    FileInfoList result;
    for (;;)
    {
        nx_spl::FileInfo* const fileInfo = fileInfoIterator->next(&errorCode);

        if (fileInfo == nullptr || errorCode != nx_spl::error::NoError)
            break;

        QString urlString = QString::fromLatin1(fileInfo->url);
        if (!urlString.contains("://"))
            urlString = QUrl(dirName).toString(QUrl::RemovePath) + QString::fromLatin1(fileInfo->url);

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
    int ecode;
    int result = m_storage->fileExists(
        urlToPath(url).toLatin1().constData(),
        &ecode
    );
    if (ecode != nx_spl::error::NoError)
        return false;
    return result;
}

bool QnThirdPartyStorageResource::isDirExists(const QString& url)
{
    if (!m_valid)
        return false;

    QnMutexLocker lock(&m_mutex);
    int ecode;
    int result = m_storage->dirExists(
        urlToPath(url).toLatin1().constData(),
        &ecode
    );
    if (ecode != nx_spl::error::NoError)
        return false;
    return result;
}

qint64 QnThirdPartyStorageResource::getFileSize(const QString& url) const
{
    if (!m_valid)
        return 0;

    QnMutexLocker lock(&m_mutex);
    int ecode;
    qint64 fsize = m_storage->fileSize(
        urlToPath(url).toLatin1().constData(),
        &ecode
    );

    if (ecode != nx_spl::error::NoError)
        return -1;
    return fsize;
}
