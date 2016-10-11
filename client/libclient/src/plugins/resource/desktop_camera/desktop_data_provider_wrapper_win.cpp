#include "desktop_data_provider_wrapper.h"

#include <plugins/resource/desktop_win/desktop_resource.h>
#include <plugins/resource/desktop_win/desktop_data_provider.h>

QnDesktopDataProviderWrapper::QnDesktopDataProviderWrapper(
    QnResourcePtr res,
    QnDesktopDataProvider* owner)
    :
    QnAbstractMediaStreamDataProvider(res),
    QnAbstractDataConsumer(100),
    m_owner(owner)
{
}

QnDesktopDataProviderWrapper::~QnDesktopDataProviderWrapper()
{
    m_owner->beforeDestroyDataProvider(this);
}

bool QnDesktopDataProviderWrapper::needConfigureProvider() const
{
    QnMutexLocker mutex(&m_mutex);
    for (const auto& dataProcessor: m_dataprocessors)
    {
        if (dataProcessor->needConfigureProvider())
            return true;
    }
    return false;
}

void QnDesktopDataProviderWrapper::putData(const QnAbstractDataPacketPtr& data)
{
    const auto media = dynamic_cast<QnAbstractMediaData*>(data.get());
    if (!media)
        return;

    QnMutexLocker mutex(&m_mutex);
    for (auto dp: m_dataprocessors)
    {
        if (dp->canAcceptData())
        {
            if (media->dataType == QnAbstractMediaData::VIDEO)
            {
                auto itr = m_needKeyData.find(dp);
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
        else
        {
            m_needKeyData << dp;
        }
    }
}

void QnDesktopDataProviderWrapper::start(Priority /*priority*/)
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
