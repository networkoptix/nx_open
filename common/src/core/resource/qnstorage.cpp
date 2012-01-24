#include "qnstorage.h"
#include "utils/common/util.h"

QnStorage::QnStorage():
    QnResource(),
    m_spaceLimit(0),
    m_maxStoreTime(0),
    m_index(0)
{
    setStatus(Offline);
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

void QnStorage::setUrl(const QString& value)
{
    QnResource::setUrl(value);
    QString tmpDir = closeDirPath(value) + QString("tmp") + QString::number(rand());
    QDir dir(tmpDir);
    if (dir.exists()) {
        setStatus(Online);
        dir.remove(tmpDir);
    }
    else {
        if (dir.mkpath(tmpDir))
        {
            dir.rmdir(tmpDir);
            setStatus(Online);
        }
        else {
            setStatus(Offline);
        }
    }
}
