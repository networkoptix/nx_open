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
    QMutexLocker lock(&m_mutex);
    m_spaceLimit = value;
}

qint64 QnAbstractStorageResource::getSpaceLimit() const
{
    QMutexLocker lock(&m_mutex);
    return m_spaceLimit;
}

void QnAbstractStorageResource::setStorageType(const QString& type)
{
    QMutexLocker lock(&m_mutex);
    m_storageType = type;
}

QString QnAbstractStorageResource::getStorageType() const
{
    QMutexLocker lock(&m_mutex);
    return m_storageType;
}

void QnAbstractStorageResource::setMaxStoreTime(int timeInSeconds)
{
    QMutexLocker lock(&m_mutex);
    m_maxStoreTime = timeInSeconds;
}

int QnAbstractStorageResource::getMaxStoreTime() const
{
    QMutexLocker lock(&m_mutex);
    return m_maxStoreTime;
}

void QnAbstractStorageResource::setUsedForWriting(bool isUsedForWriting) 
{
    QMutexLocker lock(&m_mutex);
    m_usedForWriting = isUsedForWriting;
}

bool QnAbstractStorageResource::isUsedForWriting() const 
{
    QMutexLocker lock(&m_mutex);
    return m_usedForWriting;
}

QString QnAbstractStorageResource::getUniqueId() const
{
    return getId().toString();
}

#ifdef ENABLE_DATA_PROVIDERS
float QnAbstractStorageResource::bitrate() const
{
    float rez = 0;
    QMutexLocker lock(&m_bitrateMtx);
    for(const QnAbstractMediaStreamDataProvider* provider: m_providers)
        rez += provider->getBitrateMbps();
    return rez;
}

void QnAbstractStorageResource::addBitrate(QnAbstractMediaStreamDataProvider* provider)
{
    QMutexLocker lock(&m_bitrateMtx);
    m_providers << provider;
}

void QnAbstractStorageResource::releaseBitrate(QnAbstractMediaStreamDataProvider* provider)
{
    QMutexLocker lock(&m_bitrateMtx);
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
