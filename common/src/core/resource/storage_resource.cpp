#include "storage_resource.h"
#include "nx/streaming/abstract_media_stream_data_provider.h"

#include <core/resource/media_server_resource.h>

#include <nx/utils/log/log.h>

const qint64 QnStorageResource::kNasStorageLimit = 50LL * 1024 * 1024 * 1024; // 50 gb
const qint64 QnStorageResource::kThirdPartyStorageLimit = 10LL * 1024 * 1024 * 1024; // 10 gb

QnStorageResource::QnStorageResource(QnCommonModule* commonModule):
    base_type(commonModule),
    m_spaceLimit(0),
    m_maxStoreTime(0),
    m_usedForWriting(false),
    m_storageBitrateCoeff(0.0),
    m_isBackup(false)
{
    addFlags(Qn::remote);
}

QnStorageResource::~QnStorageResource()
{
}

QnMediaServerResourcePtr QnStorageResource::getParentServer() const {
    return getParentResource().dynamicCast<QnMediaServerResource>();
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

void QnStorageResource::setUsedForWriting(bool isUsedForWriting) {
    {
        QnMutexLocker lock(&m_mutex);
        if (m_usedForWriting == isUsedForWriting)
            return;
        m_usedForWriting = isUsedForWriting;
    }
    emit isUsedForWritingChanged(::toSharedPointer(this));
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
    QnMutexLocker lock(&m_bitrateMtx);
    for(const QnAbstractMediaStreamDataProvider* provider: m_providers)
        rez += provider->getBitrateMbps();
    return rez;
}

void QnStorageResource::addBitrate(QnAbstractMediaStreamDataProvider* provider)
{
    QnMutexLocker lock(&m_bitrateMtx);
    m_providers << provider;
}

void QnStorageResource::releaseBitrate(QnAbstractMediaStreamDataProvider* provider)
{
    QnMutexLocker lock(&m_bitrateMtx);
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

QString QnStorageResource::urlWithoutCredentials(const QString& url)
{
    if (!url.contains(lit("://")))
        return url;

    QUrl result(url);
    result.setUserName(QString());
    result.setPassword(QString());

    return result.toString();
}

float QnStorageResource::getAvarageWritingUsage() const
{
    return 0.0;
}

void QnStorageResource::updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers)
{
    NX_ASSERT(other->getParentId() == getParentId() && other->getUrl() == getUrl());

    base_type::updateInternal(other, notifiers);

    QnStorageResource* localOther = dynamic_cast<QnStorageResource*>(other.data());
    if (localOther)
    {
        if (m_usedForWriting != localOther->m_usedForWriting)
        {
            m_usedForWriting = localOther->m_usedForWriting;
            notifiers << [r = toSharedPointer(this)]{emit r->isUsedForWritingChanged(r);};
        }

        if (m_isBackup != localOther->m_isBackup)
        {
            m_isBackup = localOther->m_isBackup;
            notifiers << [r = toSharedPointer(this)]{ emit r->isBackupChanged(r); };
        }

        m_spaceLimit = localOther->m_spaceLimit;
        m_maxStoreTime = localOther->m_maxStoreTime;
    }
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

void QnStorageResource::setBackup(bool value) {
    {
        QnMutexLocker lk(&m_mutex);
        if (m_isBackup == value)
            return;
        m_isBackup = value;
    }
    emit isBackupChanged(::toSharedPointer(this));
}

bool QnStorageResource::isBackup() const
{
    QnMutexLocker lk(&m_mutex);
    return m_isBackup;
}

bool QnStorageResource::isWritable() const {
    return (getCapabilities() & QnAbstractStorageResource::cap::WriteFile) == QnAbstractStorageResource::cap::WriteFile;
}
