#include <stdexcept>
#include <memory>
#include "third_party_storage_resource.h"
#include "media_server/settings.h"
#include "utils/common/buffered_file.h"
#include "utils/fs/file.h"

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
    const QString               &url,
    const StorageFactoryPtrType &sf
)
{
    try
    {
        return new QnThirdPartyStorageResource(
            sf,
            url
        );
    }
    catch(...)
    {
        return new QnThirdPartyStorageResource;
    }
}

QnThirdPartyStorageResource::QnThirdPartyStorageResource(
        const StorageFactoryPtrType &sf,
        const QString               &storageUrl
) : m_valid(true)
{
    openStorage(
        storageUrl.toLatin1().constData(),
        sf
    );
}

QnThirdPartyStorageResource::QnThirdPartyStorageResource()
    : m_valid(false)
{}

QnThirdPartyStorageResource::~QnThirdPartyStorageResource()
{}

void QnThirdPartyStorageResource::openStorage(
    const char                  *storageUrl,
    const StorageFactoryPtrType &sf
)
{
    QnMutexLocker lock(&m_mutex);

    int ecode;
    nx_spl::Storage* spRaw = sf->createStorage(storageUrl, &ecode);

    if (ecode != nx_spl::error::NoError)
    {
        if (spRaw != nullptr)
            spRaw->releaseRef();
        throw std::runtime_error("Couldn't create Storage");
    }

    if (spRaw == nullptr)
        throw std::runtime_error("Couldn't create Storage");

    std::shared_ptr<nx_spl::Storage> sp =
        std::shared_ptr<nx_spl::Storage>(
            spRaw,
            [](nx_spl::Storage* p)
            {
                p->releaseRef();
            }
        );

    void* queryResult = nullptr;
    if ((queryResult = sp->queryInterface(nx_spl::IID_Storage)))
    {
        m_storage.reset(
            static_cast<nx_spl::Storage*>(queryResult),
            [](nx_spl::Storage* p)
            {
                p->releaseRef();
            }
        );
    }
    else
    {
        throw std::logic_error("Unknown storage interface version");
    }
}

QIODevice *QnThirdPartyStorageResource::open(
    const QString       &fileName,
    QIODevice::OpenMode  openMode
)
{
    if (!m_valid)
        return nullptr;

    if (fileName.isEmpty())
        return nullptr;

    int ecode;
    int ioFlags = 0;

    if (openMode & QIODevice::ReadOnly)
        ioFlags |= nx_spl::io::ReadOnly;
    if (openMode & QIODevice::WriteOnly)
        ioFlags |= nx_spl::io::WriteOnly;

    nx_spl::IODevice* ioRaw = m_storage->open(
        QUrl(fileName).path().toLatin1().constData(),
        ioFlags,
        &ecode
    );

    if (ioRaw == nullptr)
        return nullptr;
    else if (ecode != nx_spl::error::NoError)
    {
        ioRaw->releaseRef();
        return nullptr;
    }

    void *queryResult = ioRaw->queryInterface(nx_spl::IID_IODevice);
    if (queryResult == nullptr)
    {
        ioRaw->releaseRef();
        return nullptr;
    }
    else
    {
        ioRaw->releaseRef();

        int ioBlockSize = 0;
        int ffmpegBufferSize = 0;

        int ffmpegMaxBufferSize =
            MSSettings::roSettings()->value(
                nx_ms_conf::MAX_FFMPEG_BUFFER_SIZE,
                nx_ms_conf::DEFAULT_MAX_FFMPEG_BUFFER_SIZE).toInt();

        if (openMode & QIODevice::WriteOnly)
        {
            ioBlockSize = MSSettings::roSettings()->value(
                nx_ms_conf::IO_BLOCK_SIZE,
                nx_ms_conf::DEFAULT_IO_BLOCK_SIZE
            ).toInt();

            ffmpegBufferSize = MSSettings::roSettings()->value(
                nx_ms_conf::FFMPEG_BUFFER_SIZE,
                nx_ms_conf::DEFAULT_FFMPEG_BUFFER_SIZE
            ).toInt();
        }
        std::unique_ptr<QBufferedFile> rez(
            new QBufferedFile(
                std::shared_ptr<IQnFile>(
                    new aux::ThirdPartyFile(
                        fileName,
                        std::shared_ptr<aux::ThirdPartyIODevice>(
                            new aux::ThirdPartyIODevice(
                                IODevicePtrType(
                                    static_cast<nx_spl::IODevice*>(queryResult),
                                    [](nx_spl::IODevice *p)
                                    {
                                        p->releaseRef();
                                    }
                                ),
                                openMode
                            )
                        )
                    )
                ),
                ioBlockSize,
                ffmpegBufferSize,
                ffmpegMaxBufferSize,
                getId()
            )
        );
        if (!rez->open(openMode))
            return nullptr;
        return rez.release();
    }
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
        return QnAbstractStorageResource::FileInfoList();

    QnMutexLocker lock(&m_mutex);
    int ecode;
    nx_spl::FileInfoIterator* fitRaw = m_storage->getFileIterator(
        urlToPath(dirName).toLatin1().constData(),
        &ecode
    );
    if (fitRaw == nullptr)
        return QnAbstractStorageResource::FileInfoList();

    if (ecode != nx_spl::error::NoError)
    {
        fitRaw->releaseRef();
        return QnAbstractStorageResource::FileInfoList();
    }

    void *queryResult = fitRaw->queryInterface(nx_spl::IID_FileInfoIterator);
    if (queryResult == nullptr)
    {
        fitRaw->releaseRef();
        return QnAbstractStorageResource::FileInfoList();
    }
    else
    {
        FileInfoIteratorPtrType fit = FileInfoIteratorPtrType(
            static_cast<nx_spl::FileInfoIterator*>(queryResult),
            [](nx_spl::FileInfoIterator *p)
            {
                p->releaseRef();
            }
        );
        fitRaw->releaseRef();

        if (!fit)
            return QnAbstractStorageResource::FileInfoList();

        QnAbstractStorageResource::FileInfoList ret;
        while (true)
        {
            int ecode;
            nx_spl::FileInfo *fi = fit->next(&ecode);

            if (fi == nullptr || ecode != nx_spl::error::NoError)
                break;

            QString urlString = QString::fromLatin1(fi->url);
            if (!urlString.contains("://"))
                urlString = QUrl(dirName).toString(QUrl::RemovePath) + QString::fromLatin1(fi->url);

            ret.append(
                QnAbstractStorageResource::FileInfo(
                    urlString,
                    fi->size,
                    (fi->type & nx_spl::isDir) ? true : false
                )
            );
        }
        return ret;
    }
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
