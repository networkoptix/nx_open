#include "desktop_data_provider_wrapper.h"

#ifdef Q_OS_WIN

#include "desktop_resource.h"
#include "desktop_data_provider.h"

QnDesktopDataProviderWrapper::QnDesktopDataProviderWrapper(QnResourcePtr res, QnDesktopDataProvider* owner):
    QnAbstractMediaStreamDataProvider(res), 
    QnAbstractDataConsumer(100),
    m_owner(owner)
{

}

QnDesktopDataProviderWrapper::~QnDesktopDataProviderWrapper()
{
    m_owner->beforeDestroyDataProvider(this);
}

void QnDesktopDataProviderWrapper::putData(const QnAbstractDataPacketPtr& data)
{
    const QnAbstractMediaData* media  = dynamic_cast<QnAbstractMediaData*>(data.data());
    if (!media)
        return;

    QMutexLocker mutex(&m_mutex);
    for (int i = 0; i < m_dataprocessors.size(); ++i)
    {
        QnAbstractDataReceptor* dp = m_dataprocessors.at(i);
        if (dp->canAcceptData()) 
        {
            if (media->dataType == QnAbstractMediaData::VIDEO)
            {
                QSet<void*>::iterator itr = m_needKeyData.find(dp);
                if (itr != m_needKeyData.end())
                {
                    if (media->flags | AV_PKT_FLAG_KEY)
                        m_needKeyData.erase(itr);
                    else
                        continue; // skip data
                }
            }
            dp->putData(data);
        }
        else {
            m_needKeyData << dp;
        }
    }
}

void QnDesktopDataProviderWrapper::start(Priority priority)
{
    m_owner->start();
}

bool QnDesktopDataProviderWrapper::isInitialized() const
{
    return m_owner->isInitialized();
}

QString QnDesktopDataProviderWrapper::lastErrorStr() const
{
    return m_owner->lastErrorStr();
}
#else

QnDesktopDataProviderWrapper::QnDesktopDataProviderWrapper(QnResourcePtr res, QnDesktopDataProvider* owner):
    QnAbstractMediaStreamDataProvider(res),
    QnAbstractDataConsumer(100) { Q_UNUSED(owner) }
QnDesktopDataProviderWrapper::~QnDesktopDataProviderWrapper(){}
void QnDesktopDataProviderWrapper::putData(const QnAbstractDataPacketPtr &data) {Q_UNUSED(data)}
void QnDesktopDataProviderWrapper::start(Priority priority) {Q_UNUSED(priority)}
bool QnDesktopDataProviderWrapper::isInitialized() const { return false; }
QString QnDesktopDataProviderWrapper::lastErrorStr() const { return QString(); }

#endif
