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
            uint32_t fsize = m_io->size(&ecode);
            
            if (ecode != Qn::error::NoError)
                return -1;

            qint64 readSize = maxlen > fsize ? fsize : maxlen;
            m_io->read(data, readSize, &ecode);

            if (ecode != Qn::error::NoError)
                return -1;
            
            return readSize;
        }

        qint64 writeData(const char *data, qint64 len) override
        {
            if (!m_io)
                return -1;

            int ecode;
            m_io->write(data, len, &ecode);

            if (ecode != Qn::error::NoError)
                return -1;
            
            return len;
        }
 
        qint64 size() const override
        {
            int ecode;
            qint64 fsize = m_io->size(&ecode);
            if (ecode != Qn::error::NoError)
                return -1;
            return fsize;
        }

        bool seek(qint64 pos) override
        {
            if (!QIODevice::seek(pos))
                return false;
            int ecode;
            int result = m_io->seek(pos, &ecode);
            if (!result || ecode != Qn::error::NoError)
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
            unsigned int                systemDependentFlags = 0
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

        virtual bool truncate( qint64 newFileSize) override
        {
            assert(false);
            return false;
        }

        virtual bool realFile() const override {return false;}
    private:
        QString         m_filename;
        TPIODevicePtr   m_impl;
    };
} //namespace aux

QnStorageResource* QnThirdPartyStorageResource::instance(const QString& url)
{
    int pIndex = url.indexOf(lit("://"));
    if (pIndex == -1)
        return nullptr;
    
    QString proto(url.left(pIndex));
    QDir pluginDir(lit("plugins"));

    for (const auto& fi : pluginDir.entryInfoList(QDir::Files))
    {
        if (fi.fileName().indexOf(proto) != -1)
        {
            try
            {
                return new QnThirdPartyStorageResource(
                    fi.absoluteFilePath(),
                    url
                );
            }
            catch(const std::exception&)
            {
                return nullptr;
            }
        }
    }
    return nullptr;
}

QnThirdPartyStorageResource::QnThirdPartyStorageResource(
    const QString  &libraryPath,
    const QString  &storageUrl
) : m_lib(libraryPath.toLatin1().constData())
{
    if (libraryPath.isEmpty() || storageUrl.isEmpty())
        throw std::runtime_error("Invalid storage construction arguments");

    if (!m_lib)
        throw std::runtime_error("Couldn't load storage plugin");

    m_csf = static_cast<create_qn_storage_factory_function>(
        m_lib.symbol("create_qn_storage_factory")
    );
    
    m_emf = static_cast<qn_storage_error_message_function>(
        m_lib.symbol("qn_storage_error_message")
    );

    if (m_csf == nullptr || m_emf == nullptr)
        throw std::runtime_error("Couldn't load library functions");

    openStorage(storageUrl.toLatin1().constData());
}

QnThirdPartyStorageResource::~QnThirdPartyStorageResource()
{}

void QnThirdPartyStorageResource::openStorage(const char *storageUrl)
{
    QMutexLocker lock(&m_mutex);
    Qn::StorageFactory* sfRaw = m_csf();
    if (sfRaw == nullptr)
        throw std::runtime_error("Couldn't create StorageFactory");

    std::shared_ptr<Qn::StorageFactory> sf = 
        std::shared_ptr<Qn::StorageFactory>(
            sfRaw, 
            [](Qn::StorageFactory* p)
            {
                p->releaseRef();
            }
        );

    int ecode;
    Qn::Storage* spRaw = sf->createStorage(storageUrl, &ecode);

    if (ecode != Qn::error::NoError)
    {
        if (spRaw != nullptr)
            spRaw->releaseRef();
        throw std::runtime_error("Couldn't create Storage");
    }

    if (spRaw == nullptr)
        throw std::runtime_error("Couldn't create Storage");

    std::shared_ptr<Qn::Storage> sp =
        std::shared_ptr<Qn::Storage>(
            spRaw,
            [](Qn::Storage* p)
            {
                p->releaseRef();
            }
        );

    void* queryResult = nullptr;
    if (queryResult = sp->queryInterface(Qn::IID_Storage))
    {
        m_storage.reset(
            static_cast<Qn::Storage*>(queryResult),
            [](Qn::Storage* p)
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
    QMutexLocker lock(&m_mutex);
    if (fileName.isEmpty())
        return nullptr;

    int ecode;
    int ioFlags = 0;

    if (openMode & QIODevice::ReadOnly)
        ioFlags |= Qn::io::ReadOnly;
    if (openMode & QIODevice::WriteOnly)
        ioFlags |= Qn::io::WriteOnly;

    Qn::IODevice* ioRaw = m_storage->open(
        fileName.toLatin1().data(), 
        ioFlags, 
        &ecode
    );

    if (ioRaw == nullptr)
        return nullptr;
    else if (ecode != Qn::error::NoError)
    {
        ioRaw->releaseRef();
        return nullptr;
    }

    void *queryResult = ioRaw->queryInterface(Qn::IID_IODevice);
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
                                    static_cast<Qn::IODevice*>(queryResult),
                                    [](Qn::IODevice *p)
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
                ffmpegBufferSize
            ) 
        );
        if (!rez->open(openMode))
            return nullptr;
        return rez.release();
    }
}

int QnThirdPartyStorageResource::getCapabilities() const
{
    QMutexLocker lock(&m_mutex);
    return m_storage->getCapabilities();
}
    
qint64 QnThirdPartyStorageResource::getFreeSpace()
{
    QMutexLocker lock(&m_mutex);
    int ecode;
    qint64 freeSpace = m_storage->getFreeSpace(&ecode);
    if (ecode != Qn::error::NoError)
        return -1;
    return freeSpace;
}

qint64 QnThirdPartyStorageResource::getTotalSpace()
{
    QMutexLocker lock(&m_mutex);
    int ecode;
    qint64 totalSpace = m_storage->getTotalSpace(&ecode);
    if (ecode != Qn::error::NoError)
        return -1;
    return totalSpace;
}

bool QnThirdPartyStorageResource::isAvailable() const
{
    QMutexLocker lock(&m_mutex);
    return m_storage->isAvailable();
}

bool QnThirdPartyStorageResource::removeFile(const QString& url)
{
    QMutexLocker lock(&m_mutex);
    int ecode;
    m_storage->removeFile(url.toLatin1().data(), &ecode);
    if (ecode != Qn::error::NoError)
        return false;
    return true;
}

bool QnThirdPartyStorageResource::removeDir(const QString& url)
{
    QMutexLocker lock(&m_mutex);
    int ecode;
    m_storage->removeDir(url.toLatin1().data(), &ecode);
    if (ecode != Qn::error::NoError)
        return false;
    return true;
}

bool QnThirdPartyStorageResource::renameFile(
    const QString   &oldName, 
    const QString   &newName
)
{
    QMutexLocker lock(&m_mutex);
    int ecode;
    m_storage->renameFile(
        oldName.toLatin1().data(), 
        newName.toLatin1().data(), 
        &ecode
    );
    if (ecode != Qn::error::NoError)
        return false;
    return true;
}


QnAbstractStorageResource::FileInfoList 
QnThirdPartyStorageResource::getFileList(const QString& dirName)
{
    QMutexLocker lock(&m_mutex);
    int ecode;
    Qn::FileInfoIterator* fitRaw = m_storage->getFileIterator(
        dirName.toLatin1().data(), 
        &ecode
    );
    if (fitRaw == nullptr)
        return QnAbstractStorageResource::FileInfoList();

    if (ecode != Qn::error::NoError)
    {
        fitRaw->releaseRef();
        return QnAbstractStorageResource::FileInfoList();
    }

    void *queryResult = fitRaw->queryInterface(Qn::IID_FileInfoIterator);
    if (queryResult == nullptr)
    {
        fitRaw->releaseRef();
        return QnAbstractStorageResource::FileInfoList();
    }
    else
    {
        FileInfoIteratorPtrType fit = FileInfoIteratorPtrType(
            static_cast<Qn::FileInfoIterator*>(queryResult),
            [](Qn::FileInfoIterator *p)
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
            Qn::FileInfo *fi = fit->next(&ecode);

            if (fi == nullptr || ecode != Qn::error::NoError)
                break;
            
            ret.append(
                QnAbstractStorageResource::FileInfo(
                    QString::fromLatin1(fi->url), 
                    fi->size, 
                    (fi->type & Qn::isDir) ? true : false
                )
            );
        }
        return ret;
    }
}

bool QnThirdPartyStorageResource::isFileExists(const QString& url)
{
    QMutexLocker lock(&m_mutex);
    int ecode;
    int result = m_storage->fileExists(url.toLatin1().constData(), &ecode);
    if (ecode != Qn::error::NoError)
        return false;
    return result;
}

bool QnThirdPartyStorageResource::isDirExists(const QString& url)
{
    QMutexLocker lock(&m_mutex);
    int ecode;
    int result = m_storage->dirExists(url.toLatin1().constData(), &ecode);
    if (ecode != Qn::error::NoError)
        return false;
    return result;
}

qint64 QnThirdPartyStorageResource::getFileSize(const QString& url) const
{
    QMutexLocker lock(&m_mutex);
    int ecode;
    qint64 fsize = m_storage->fileSize(url.toLatin1().constData(), &ecode);

    if (ecode != Qn::error::NoError)
        return -1;
    return fsize;
}