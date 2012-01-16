#include "qnstorage.h"

QnStorage::QnStorage():
    QnResource(),
    m_spaceLimit(0),
    m_maxStoreTime(0),
    m_index(0)
{

}

QnStorage::~QnStorage()
{

}

void QnStorage::setSpaceLimit(qint64 value)
{
    m_spaceLimit = value;
}

qint64 QnStorage::getSpaceLimit() const
{
    return m_spaceLimit;
}

void QnStorage::setMaxStoreTime(int timeInSeconds)
{
    m_maxStoreTime = timeInSeconds;
}

int QnStorage::getMaxStoreTime() const
{
    return m_maxStoreTime;
}

QString QnStorage::getUniqueId() const
{
    return QString("storage://") + getUrl();
}

void QnStorage::setIndex(quint16 value)
{
    m_index = value;
}

quint16 QnStorage::getIndex() const
{
    return m_index;
}
