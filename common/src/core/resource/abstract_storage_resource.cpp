#include "abstract_storage_resource.h"
#include "core/dataprovider/media_streamdataprovider.h"

QnAbstractStorageResource::QnAbstractStorageResource():
    QnResource(),
    m_spaceLimit(0),
    m_maxStoreTime(0),
    m_usedForWriting(false)
{
    setStatus(Qn::Offline);
}

QnAbstractStorageResource::~QnAbstractStorageResource()
{
}

void QnAbstractStorageResource::setSpaceLimit(qint64 value)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    m_spaceLimit = value;
}

qint64 QnAbstractStorageResource::getSpaceLimit() const
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    return m_spaceLimit;
}

void QnAbstractStorageResource::setMaxStoreTime(int timeInSeconds)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    m_maxStoreTime = timeInSeconds;
}

int QnAbstractStorageResource::getMaxStoreTime() const
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    return m_maxStoreTime;
}

void QnAbstractStorageResource::setUsedForWriting(bool isUsedForWriting) 
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    m_usedForWriting = isUsedForWriting;
}

bool QnAbstractStorageResource::isUsedForWriting() const 
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    return m_usedForWriting;
}

QString QnAbstractStorageResource::getUniqueId() const
{
    return QLatin1String("storage://") + getUrl();
}

#ifdef ENABLE_DATA_PROVIDERS
float QnAbstractStorageResource::bitrate() const
{
    float rez = 0;
    SCOPED_MUTEX_LOCK( lock, &m_bitrateMtx);
    for(const QnAbstractMediaStreamDataProvider* provider: m_providers)
        rez += provider->getBitrate();
    return rez;
}

void QnAbstractStorageResource::addBitrate(QnAbstractMediaStreamDataProvider* provider)
{
    SCOPED_MUTEX_LOCK( lock, &m_bitrateMtx);
    m_providers << provider;
}

void QnAbstractStorageResource::releaseBitrate(QnAbstractMediaStreamDataProvider* provider)
{
    SCOPED_MUTEX_LOCK( lock, &m_bitrateMtx);
    m_providers.remove(provider);
}
#endif

QString QnAbstractStorageResource::getPath() const
{
    return urlToPath(getUrl());
}

QString QnAbstractStorageResource::urlToPath(const QString& url)
{
    if (!url.contains(lit("://")))
        return url;
    else
        return QUrl(url).path().mid(1);
}

float QnAbstractStorageResource::getAvarageWritingUsage() const
{
    return 0.0;
}

QnAbstractStorageResource::ProtocolDescription QnAbstractStorageResource::protocolDescription(const QString &protocol) {
    ProtocolDescription result;

    QString validIpPattern = lit("(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])");
    QString validHostnamePattern = lit("(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-9])");

    if(protocol == lit("smb")) {
        result.protocol = protocol;
        result.name = tr("Windows Network Shared Resource");
        result.urlTemplate = tr("\\\\<Computer Name>\\<Folder>");
        result.urlPattern = lit("\\\\\\\\%1\\\\.+").arg(validHostnamePattern);
    } else if(protocol == lit("coldstore")) {
        result.protocol = protocol;
        result.name = tr("Coldstore Network Storage");
        result.urlTemplate = tr("coldstore://<Address>");
        result.urlPattern = lit("coldstore://%1/?").arg(validHostnamePattern);
    }

    return result;
}

void QnAbstractStorageResource::updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields)
{
    Q_ASSERT(other->getParentId() == getParentId() && other->getUrl() == getUrl());
    QnResource::updateInner(other, modifiedFields);

    QnAbstractStorageResourcePtr storage = other.dynamicCast<QnAbstractStorageResource>();

    m_spaceLimit = storage->m_spaceLimit;
    m_maxStoreTime = storage->m_maxStoreTime;
    m_usedForWriting = storage->m_usedForWriting;
}

void QnAbstractStorageResource::setUrl(const QString& value)
{
    QnResource::setUrl(value);
    if (getId().isNull() && !getParentId().isNull()) 
        setId(fillID(getParentId(), value));
}

QnUuid QnAbstractStorageResource::fillID(const QnUuid& mserverId, const QString& url)
{
    QByteArray data1 = mserverId.toRfc4122();
    QByteArray data2 = url.toUtf8();
    return guidFromArbitraryData(data1.append(data2));
}

bool QnAbstractStorageResource::isExternal() const
{
    QString storageUrl = getUrl();
    return storageUrl.trimmed().startsWith(lit("\\\\")) || QUrl(storageUrl).path().mid(1).startsWith(lit("\\\\"));
}
