#include "storage_resource.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "utils/common/log.h"

QnStorageResource::QnStorageResource():
    m_spaceLimit(0),
    m_maxStoreTime(0),
    m_usedForWriting(false)
{
    setStatus(Qn::Offline);
}

QnStorageResource::~QnStorageResource()
{
}

void QnStorageResource::setSpaceLimit(qint64 value)
{
    QnMutexLocker lock(&m_mutex);
    m_spaceLimit = value;
}

qint64 QnStorageResource::getSpaceLimit() const
{
    QnMutexLocker lock(&m_mutex);
    return m_spaceLimit;
}

void QnStorageResource::setStorageType(const QString& type)
{
    QnMutexLocker lock(&m_mutex);
    m_storageType = type;
}

QString QnStorageResource::getStorageType() const
{
    QnMutexLocker lock(&m_mutex);
    return m_storageType;
}

void QnStorageResource::setMaxStoreTime(int timeInSeconds)
{
    QnMutexLocker lock(&m_mutex);
    m_maxStoreTime = timeInSeconds;
}

int QnStorageResource::getMaxStoreTime() const
{
    QnMutexLocker lock(&m_mutex);
    return m_maxStoreTime;
}

void QnStorageResource::setUsedForWriting(bool isUsedForWriting) 
{
    QnMutexLocker lock(&m_mutex);
    m_usedForWriting = isUsedForWriting;
}

bool QnStorageResource::isUsedForWriting() const 
{
    QnMutexLocker lock(&m_mutex);
    return m_usedForWriting;
}

QString QnStorageResource::getUniqueId() const
{
    return getId().toString();
}

#ifdef ENABLE_DATA_PROVIDERS
float QnStorageResource::bitrate() const
{
    float rez = 0;
    QMutexLocker lock(&m_bitrateMtx);
    for(const QnAbstractMediaStreamDataProvider* provider: m_providers)
        rez += provider->getBitrateMbps();
    return rez;
}

void QnStorageResource::addBitrate(QnAbstractMediaStreamDataProvider* provider)
{
    QMutexLocker lock(&m_bitrateMtx);
    m_providers << provider;
}

void QnStorageResource::releaseBitrate(QnAbstractMediaStreamDataProvider* provider)
{
    QMutexLocker lock(&m_bitrateMtx);
    m_providers.remove(provider);
}
#endif

QString QnStorageResource::getPath() const
{
    return getUrl();//urlToPath(getUrl());
}

void QnStorageResource::setStorageBitrateCoeff(float value)
{
    NX_LOG(lit("QnFileStorageResource %1 coeff %2").arg(getPath()).arg(value), cl_logDEBUG2);
    m_storageBitrateCoeff = value;
}

QString QnStorageResource::urlToPath(const QString& url)
{
    if (!url.contains(lit("://")))
        return url;
    else
        return QUrl(url).path();
}

float QnStorageResource::getAvarageWritingUsage() const
{
    return 0.0;
}

void QnStorageResource::updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields)
{
    Q_ASSERT(other->getParentId() == getParentId() && other->getUrl() == getUrl());
    QnResource::updateInner(other, modifiedFields);

    QnStorageResourcePtr storage = other.dynamicCast<QnStorageResource>();

    m_spaceLimit = storage->m_spaceLimit;
    m_maxStoreTime = storage->m_maxStoreTime;
    m_usedForWriting = storage->m_usedForWriting;
}

void QnStorageResource::setUrl(const QString& value)
{
    QnResource::setUrl(value);
    if (getId().isNull() && !getParentId().isNull()) 
        setId(fillID(getParentId(), value));
}

QnUuid QnStorageResource::fillID(const QnUuid& mserverId, const QString& url)
{
    QByteArray data1 = mserverId.toRfc4122();
    QByteArray data2 = url.toUtf8();
    return guidFromArbitraryData(data1.append(data2));
}

bool QnStorageResource::isExternal() const
{
    QString storageUrl = getUrl();
    return 
        storageUrl.trimmed().startsWith(lit("\\\\"))            || 
        QUrl(storageUrl).path().mid(1).startsWith(lit("\\\\"))  ||
        storageUrl.indexOf(lit("://")) != -1;
}

QString QnStorageResource::toNativeDirPath(const QString &dirPath)
{
    QString result = QDir::toNativeSeparators(dirPath);
    if(!result.endsWith(QDir::separator()))
        result.append(QDir::separator());
    return result;
}
