#include "desktop_data_provider_wrapper.h"
#include "device/desktop_resource.h"

QnDesktopDataProviderWrapper::QnDesktopDataProviderWrapper(QnResourcePtr res):
    QnAbstractMediaStreamDataProvider(res), 
    QnAbstractDataConsumer(100)
{

}

QnDesktopDataProviderWrapper::~QnDesktopDataProviderWrapper()
{

}

void QnDesktopDataProviderWrapper::putData(QnAbstractDataPacketPtr data)
{
    QnAbstractMediaDataPtr media  = data.dynamicCast<QnAbstractMediaData>();
    if (!media)
        return;

    QMutexLocker mutex(&m_mutex);
    for (int i = 0; i < m_dataprocessors.size(); ++i)
    {
        QnAbstractDataConsumer* dp = m_dataprocessors.at(i);
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
    m_resource.dynamicCast<QnDesktopResource>()->beforeStartDataProvider(this);
}

void QnDesktopDataProviderWrapper::pleaseStop()
{
    m_resource.dynamicCast<QnDesktopResource>()->beforeDestroyDataProvider(this);
}
