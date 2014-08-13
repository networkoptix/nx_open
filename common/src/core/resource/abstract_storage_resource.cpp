#include "abstract_storage_resource.h"
#include "core/dataprovider/media_streamdataprovider.h"

QnAbstractStorageResource::QnAbstractStorageResource():
    QnResource(),
    m_spaceLimit(0),
    m_maxStoreTime(0),
    m_usedForWriting(false),
    m_index(0)
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
    return QLatin1String("storage://") + getUrl();
}

void QnAbstractStorageResource::setIndex(quint16 value)
{
    QMutexLocker lock(&m_mutex);
    m_index = value;
}

quint16 QnAbstractStorageResource::getIndex() const
{
    QMutexLocker lock(&m_mutex);
    return m_index;
}

#ifdef ENABLE_DATA_PROVIDERS
float QnAbstractStorageResource::bitrate() const
{
    float rez = 0;
    QMutexLocker lock(&m_bitrateMtx);
    foreach(const QnAbstractMediaStreamDataProvider* provider, m_providers)
        rez += provider->getBitrate();
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
