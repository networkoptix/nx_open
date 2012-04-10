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

void QnStorageResource::setUrl(const QString& value)
{
    QnResource::setUrl(value);

    if (isStorageAvailable())
        setStatus(Online);
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

    QnStorageResource* tmpInstance = pluginInst();
    tmpInstance->registerFfmpegProtocol();
    delete tmpInstance;
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
