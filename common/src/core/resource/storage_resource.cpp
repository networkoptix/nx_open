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
    return QLatin1String("storage://") + getUrl();
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
	QMutexLocker lock(&m_mutex);
    foreach(const QnAbstractMediaStreamDataProvider* provider, m_providers)
        rez += provider->getBitrate();
    return rez;
}

void QnAbstractStorageResource::addBitrate(QnAbstractMediaStreamDataProvider* provider)
{
	QMutexLocker lock(&m_mutex);
    m_providers << provider;
}

void QnAbstractStorageResource::releaseBitrate(QnAbstractMediaStreamDataProvider* provider)
{
	QMutexLocker lock(&m_mutex);
    m_providers.remove(provider);
}

void QnAbstractStorageResource::deserialize(const QnResourceParameters& parameters)
{
    QnResource::deserialize(parameters);

    const char* SPACELIMIT = "spaceLimit";

    if (parameters.contains(QLatin1String(SPACELIMIT)))
        setSpaceLimit(parameters[QLatin1String(SPACELIMIT)].toLongLong());
}

float QnAbstractStorageResource::getAvarageWritingUsage() const
{
    return 0.0;
}

// ------------------------------ QnStorageResource -------------------------------

QnStorageResource::QnStorageResource():
    m_writedSpace(0)
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
    if (reader)
        return reader->write((char*) buf, size);
    else
        return 0;
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
    int prefix = url.indexOf(QLatin1String("://"));
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

AVIOContext* QnStorageResource::createFfmpegIOContext(QIODevice* ioDevice, int IO_BLOCK_SIZE)
{
    quint8* ioBuffer;
    AVIOContext* ffmpegIOContext;

    ioBuffer = (quint8*) av_malloc(IO_BLOCK_SIZE);
    ffmpegIOContext = avio_alloc_context(
        ioBuffer,
        IO_BLOCK_SIZE,
        (ioDevice->openMode() & QIODevice::WriteOnly) ? 1 : 0,
        ioDevice,
        &ffmpegReadPacket,
        &ffmpegWritePacket,
        &ffmpegSeek);

    return ffmpegIOContext;
}

qint64 QnStorageResource::getFileSizeByIOContext(AVIOContext* ioContext)
{
    if (ioContext)
    {
        QIODevice* ioDevice = (QIODevice*) ioContext->opaque;
        return ioDevice->size();
    }
    return 0;
}

void QnStorageResource::closeFfmpegIOContext(AVIOContext* ioContext)
{
    if (ioContext)
    {
        QIODevice* ioDevice = (QIODevice*) ioContext->opaque;
        delete ioDevice;
        ioContext->opaque = 0;
        avio_close(ioContext);
    }
}

qint64 QnStorageResource::getWritedSpace() const
{
    return m_writedSpace;
}

void QnStorageResource::addWritedSpace(qint64 value)
{
    m_writedSpace += value;
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

QnStorageResource* QnStoragePluginFactory::createStorage(const QString& storageType, bool useDefaultForUnknownPrefix)
{
    int prefix = storageType.indexOf(QLatin1String("://"));
    if (prefix == -1)
        return m_defaultStoragePlugin();
    QString protocol = storageType.left(prefix);
    if (m_storageTypes.contains(protocol))
        return m_storageTypes.value(protocol)();
    else {
        if (useDefaultForUnknownPrefix)
            return m_defaultStoragePlugin();
        else
            return 0;
    }
}
