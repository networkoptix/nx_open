#include "abstract_storage_resource.h"
#include "core/dataprovider/media_streamdataprovider.h"

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
