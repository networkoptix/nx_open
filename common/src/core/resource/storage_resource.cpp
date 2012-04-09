#include "storage_resource.h"
#include "utils/common/util.h"
#include "../dataprovider/media_streamdataprovider.h"

QnStorageResource::QnStorageResource():
    QnResource(),
    m_spaceLimit(0),
    m_maxStoreTime(0),
    m_index(0)
{
    setStatus(Offline);
}

QnStorageResource::~QnStorageResource()
{

}

void QnStorageResource::setSpaceLimit(qint64 value)
{
    m_spaceLimit = value;
}

qint64 QnStorageResource::getSpaceLimit() const
{
    return m_spaceLimit;
}

void QnStorageResource::setMaxStoreTime(int timeInSeconds)
{
    m_maxStoreTime = timeInSeconds;
}

int QnStorageResource::getMaxStoreTime() const
{
    return m_maxStoreTime;
}

QString QnStorageResource::getUniqueId() const
{
    return QString("storage://") + getUrl();
}

void QnStorageResource::setIndex(quint16 value)
{
    m_index = value;
}

quint16 QnStorageResource::getIndex() const
{
    return m_index;
}

void QnStorageResource::setUrl(const QString& value)
{
    QnResource::setUrl(value);

    QnAbstractStorageProtocolPtr storageProtocol = qnStorageProtocolManager->getProtocol(value);
    if (storageProtocol == 0)
        return;

    if (storageProtocol->isStorageAvailable(value))
        setStatus(Online);
}

float QnStorageResource::bitrate() const
{
    float rez = 0;
    foreach(QnAbstractMediaStreamDataProvider* provider, m_providers)
        rez += provider->getBitrate();
    return rez;
}

void QnStorageResource::addBitrate(QnAbstractMediaStreamDataProvider* provider)
{
    m_providers << provider;
}

void QnStorageResource::releaseBitrate(QnAbstractMediaStreamDataProvider* provider)
{
    m_providers.remove(provider);
}
