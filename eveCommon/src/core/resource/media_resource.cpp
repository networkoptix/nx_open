#include "media_resource.h"
#include "resource_media_layout.h"
#include "../dataprovider/media_streamdataprovider.h"


//QnDefaultMediaResourceLayout globalDefaultMediaResourceLayout;

QnMediaResource::QnMediaResource(): 
    QnResource()
{
    addFlag(QnResource::media);
}

QnMediaResource::~QnMediaResource()
{
}

/*
QnMediaResourceLayout* QnMediaResource::getMediaLayout() const
{
    return &globalDefaultMediaResourceLayout;
}
*/

QnAbstractMediaStreamDataProvider* QnMediaResource::createMediaProvider(ConnectionRole role)
{
    QnAbstractMediaStreamDataProvider* dp = dynamic_cast<QnAbstractMediaStreamDataProvider*> (getDeviceStreamConnection(role));
    Q_ASSERT(dp);
    return dp;
}

QnAbstractMediaStreamDataProvider* QnMediaResource::acquireMediaProvider(int number, ConnectionRole role)
{
    QMutexLocker locker(&m_mutex);
    //Q_ASSERT(number < getMediaLayout()->numberOfVideoChannels());

    StreamProvidersList::iterator it = m_streamProviders.find(number);

    if ( it == m_streamProviders.end() )
    {
        QnAbstractMediaStreamDataProvider* dp = createMediaProvider(role);
        m_streamProviders[number] = dp;
        return dp;
    }

    return it.value();
}

void QnMediaResource::releaseMediaProvider(int number)
{
    QMutexLocker locker(&m_mutex);
    //Q_ASSERT(number < getMediaLayout()->numberOfVideoChannels());

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
    //Q_ASSERT(number < getMediaLayout()->numberOfVideoChannels());

    StreamProvidersList::iterator it = m_streamProviders.find(number);

    if ( it == m_streamProviders.end() )
    {
        return 0;
    }

    return it.value();
}

QImage QnMediaResource::getImage(int channnel, QDateTime time, QnStreamQuality quality)
{
    return QImage();
}

QnDeviceVideoLayout* QnMediaResource::getVideoLayout(QnAbstractMediaStreamDataProvider* reader)
{
    static QnDefaultDeviceVideoLayout defaultLayout;
    if (reader)
        return reader->getVideoLayout();
    else
        return &defaultLayout;
}

QnDeviceAudioLayout* QnMediaResource::getAudioLayout(QnAbstractMediaStreamDataProvider* reader)
{
    static QnEmptyAudioLayout defaultLayout;
    if (reader)
        return reader->getAudioLayout();
    else
        return &defaultLayout;
}
