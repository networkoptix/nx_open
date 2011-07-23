#include "media_resource.h"
#include "media_resource_layout.h"



extern QnDefaultMediaResourceLayout globalDefaultMediaResourceLayout;

QnMediaResource::QnMediaResource()
{
    addFlag(QnResource::media);
}

QnMediaResource::~QnMediaResource()
{
    QMutexLocker locker(&m_mutex);
    foreach(QnAbstractMediaStreamDataProvider* dp, m_streamProviders)
    {
        dp->pleaseStop();
    }

    foreach(QnAbstractMediaStreamDataProvider* dp, m_streamProviders)
    {
        dp->stop();
        delete dp;
    }

}

QnMediaResourceLayout* QnMediaResource::getVideoLayout() const
{
    return &globalDefaultMediaResourceLayout;
}

QnAbstractMediaStreamDataProvider* QnMediaResource::acquireMediaProvider(int number)
{
    QMutexLocker locker(&m_mutex);
    Q_ASSERT(number < getVideoLayout()->numberOfChannels());

    StreamProvidersList::iterator it = m_streamProviders.find(number);

    if ( it == m_streamProviders.end() )
    {
        QnAbstractMediaStreamDataProvider* dp = createMediaProvider();
        m_streamProviders[number] = dp;
        return dp;
    }

    return it.value();
}

void QnMediaResource::releaseMediaProvider(int number)
{
    QMutexLocker locker(&m_mutex);
    Q_ASSERT(number < getVideoLayout()->numberOfChannels());

    StreamProvidersList::iterator it = m_streamProviders.find(number);

    if ( it == m_streamProviders.end() )
    {
        Q_ASSERT(false); // should not be the case
        return;
    }

    it.value()->stop();
    delete it.value();
    m_streamProviders.erase(it);
}


QnAbstractMediaStreamDataProvider* QnMediaResource::getMediaProvider(int number)
{
    QMutexLocker locker(&m_mutex);
    Q_ASSERT(number < getVideoLayout()->numberOfChannels());

    StreamProvidersList::iterator it = m_streamProviders.find(number);

    if ( it == m_streamProviders.end() )
    {
        return 0;
    }

    return it.value();
}
