#include "storage_resource.h"
#include "utils/common/util.h"
#include "../dataprovider/media_streamdataprovider.h"

QnAbstractStorageResource::QnAbstractStorageResource():
    QnResource(),
    m_spaceLimit(0),
    m_maxStoreTime(0),
    m_index(0)
{
    setStatus(Offline);
}

QnAbstractStorageResource::~QnAbstractStorageResource()
{

}

void QnAbstractStorageResource::setSpaceLimit(qint64 value)
{
    m_spaceLimit = value;
}

qint64 QnAbstractStorageResource::getSpaceLimit() const
{
    return m_spaceLimit;
}

void QnAbstractStorageResource::setMaxStoreTime(int timeInSeconds)
{
    m_maxStoreTime = timeInSeconds;
}

int QnAbstractStorageResource::getMaxStoreTime() const
{
    return m_maxStoreTime;
}

QString QnAbstractStorageResource::getUniqueId() const
{
    return QString("storage://") + getUrl();
}

void QnAbstractStorageResource::setIndex(quint16 value)
{
    m_index = value;
}

quint16 QnAbstractStorageResource::getIndex() const
{
    return m_index;
}

float QnAbstractStorageResource::bitrate() const
{
    float rez = 0;
    foreach(QnAbstractMediaStreamDataProvider* provider, m_providers)
        rez += provider->getBitrate();
    return rez;
}

void QnAbstractStorageResource::addBitrate(QnAbstractMediaStreamDataProvider* provider)
{
    m_providers << provider;
}

void QnAbstractStorageResource::releaseBitrate(QnAbstractMediaStreamDataProvider* provider)
{
    m_providers.remove(provider);
}

void QnAbstractStorageResource::deserialize(const QnResourceParameters& parameters)
{
    QnResource::deserialize(parameters);

    const char* SPACELIMIT = "spaceLimit";

    if (parameters.contains(QLatin1String(SPACELIMIT)))
        setSpaceLimit(parameters[QLatin1String(SPACELIMIT)].toLongLong());
}



// ------------------------------ QnStorageResource -------------------------------

QnStorageResource::QnStorageResource()
{
}
QnStorageResource::~QnStorageResource()
{
}

static qint32 ffmpegReadPacket(void *opaque, quint8* buf, int size)
{
    QIODevice* reader = reinterpret_cast<QIODevice*> (opaque);
    return reader->read((char*) buf, size);
}

static qint32 ffmpegWritePacket(void *opaque, quint8* buf, int size)
{
    QIODevice* reader = reinterpret_cast<QIODevice*> (opaque);
    return reader->write((char*) buf, size);
}

static int64_t ffmpegSeek(void* opaque, int64_t pos, int whence)
{
    QIODevice* reader = reinterpret_cast<QIODevice*> (opaque);

    qint64 absolutePos = pos;
    switch (whence)
    {
    case SEEK_SET: 
        break;
    case SEEK_CUR: 
        absolutePos = reader->pos() + pos;
        break;
    case SEEK_END: 
        absolutePos = reader->size() + pos;
        break;
    case 65536:
        return reader->size();
    default:
        return AVERROR(ENOENT);
    }

    return reader->seek(absolutePos);
}


void QnStorageResource::setUrl(const QString& value)
{
    QnResource::setUrl(value);
}

AVIOContext* QnStorageResource::createFfmpegIOContext(const QString& url, QIODevice::OpenMode openMode, int IO_BLOCK_SIZE)
{
    int prefix = url.indexOf("://");
    QString path = prefix == -1 ? url : url.mid(prefix+3);

    quint8* ioBuffer;
    AVIOContext* ffmpegIOContext;

    QIODevice* ioDevice = open(path, openMode);
    if (ioDevice == 0)
        return 0;

    ioBuffer = (quint8*) av_malloc(IO_BLOCK_SIZE);
    ffmpegIOContext = avio_alloc_context(
        ioBuffer,
        IO_BLOCK_SIZE,
        (openMode & QIODevice::WriteOnly) ? 1 : 0,
        ioDevice,
        &ffmpegReadPacket,
        &ffmpegWritePacket,
        &ffmpegSeek);
    
    return ffmpegIOContext;
}

void QnStorageResource::closeFfmpegIOContext(AVIOContext* ioContext)
{
    if (ioContext)
    {
        QIODevice* ioDevice = (QIODevice*) ioContext->opaque;
        ioDevice->close();
        delete ioDevice;
        ioContext->opaque = 0;
        avio_close(ioContext);
    }
}

// ---------------------------- QnStoragePluginFactory ------------------------------

Q_GLOBAL_STATIC(QnStoragePluginFactory, inst)

QnStoragePluginFactory::QnStoragePluginFactory()
{

}

QnStoragePluginFactory::~QnStoragePluginFactory()
{

}

QnStoragePluginFactory* QnStoragePluginFactory::instance()
{
    return inst();
}

void QnStoragePluginFactory::registerStoragePlugin(const QString& name, StorageTypeInstance pluginInst, bool isDefaultProtocol)
{
    m_storageTypes.insert(name, pluginInst);
    if (isDefaultProtocol)
        m_defaultStoragePlugin = pluginInst;
}

QnStorageResource* QnStoragePluginFactory::createStorage(const QString& storageType)
{
    int prefix = storageType.indexOf("://");
    if (prefix == -1)
        return m_defaultStoragePlugin();
    QString protocol = storageType.left(prefix);
    if (m_storageTypes.contains(protocol))
        return m_storageTypes.value(protocol)();
    else
        return m_defaultStoragePlugin();
}
