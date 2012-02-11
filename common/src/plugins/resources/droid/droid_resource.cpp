#include "droid_resource.h"
#include "droid_stream_reader.h"


const char* QnDroidResource::MANUFACTURE = "NetworkOptixDroid";

QnDroidResource::QnDroidResource()
{
}

int QnDroidResource::getMaxFps()
{
    return 30;
}

bool QnDroidResource::isResourceAccessible()
{
    return true;
}

bool QnDroidResource::updateMACAddress()
{
    return true;
}

QString QnDroidResource::manufacture() const
{
    return MANUFACTURE;
}

void QnDroidResource::setIframeDistance(int frames, int timems)
{

}

QnAbstractStreamDataProvider* QnDroidResource::createLiveDataProvider()
{
    return new PlDroidStreamReader(toSharedPointer());;
}

void QnDroidResource::setCropingPhysical(QRect croping)
{

}



void QnDroidResource::setDataPort(int port)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_dataPort = port;
}

int QnDroidResource::getDataPort() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_dataPort;
}

void QnDroidResource::setVideoPort(int port)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_videoPort = port;
}

int QnDroidResource::getVideoPort() const
{
    QMutexLocker mutexLocker(&m_mutex);
    return m_videoPort;
}
