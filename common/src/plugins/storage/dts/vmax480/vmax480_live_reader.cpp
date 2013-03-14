#include "vmax480_live_reader.h"
#include "core/resource/network_resource.h"
#include "core/datapacket/media_data_packet.h"
#include "libavcodec/avcodec.h"
#include "utils/common/sleep.h"
#include "utils/common/synctime.h"

#include "vmax480_resource.h"

static const QByteArray GROUP_ID("{347E1C92-4627-405d-99B3-5C7EF78B0055}");

// ----------------------------------- QnVMax480LiveProvider -----------------------

QnVMax480LiveProvider::QnVMax480LiveProvider(QnResourcePtr dev ):
    CLServerPushStreamreader(dev),
    m_maxStream(0),
    m_opened(false)
{
    m_networkRes = dev.dynamicCast<QnNetworkResource>();
}

QnVMax480LiveProvider::~QnVMax480LiveProvider()
{
    closeStream();
}

QnAbstractMediaDataPtr QnVMax480LiveProvider::getNextData()
{
    if (!isStreamOpened()) {
        openStream();
        if (!isStreamOpened())
            return QnAbstractMediaDataPtr(0);
    }

    if (needMetaData())
        return getMetaData();

    QnAbstractDataPacketPtr result;
    QTime getTimer;
    getTimer.restart();
    while (!needToStop() && isStreamOpened() && getTimer.elapsed() < MAX_FRAME_DURATION * 2 && !result)
    {
        result = m_maxStream->getNextData(this);
    }

    if (!result)
        closeStream();
    return result.dynamicCast<QnAbstractMediaData>();
}

void QnVMax480LiveProvider::openStream()
{
    if (m_opened)
        return;

    int channel = QUrl(m_resource->getUrl()).queryItemValue(QLatin1String("channel")).toInt();
    if (channel > 0)
        channel--;


    if (m_maxStream == 0)
        m_maxStream = VMaxStreamFetcher::getInstance(GROUP_ID, m_resource, true);
    m_opened = m_maxStream->registerConsumer(this); 

    /*
    vmaxDisconnect();
    vmaxConnect(true, channel);
    */
}

void QnVMax480LiveProvider::closeStream()
{
    if (m_opened) {
        m_opened = false;
        m_maxStream->unregisterConsumer(this);
    }
    if (m_maxStream)
        m_maxStream->freeInstance(GROUP_ID, m_resource, true);
    m_maxStream = 0;

    //vmaxDisconnect();
}

bool QnVMax480LiveProvider::isStreamOpened() const
{
    return m_opened;
}

void QnVMax480LiveProvider::beforeRun()
{
    //msleep(300);
}

void QnVMax480LiveProvider::afterRun()
{
    //msleep(300);
    closeStream();
}

void QnVMax480LiveProvider::updateStreamParamsBasedOnQuality()
{
    if (isRunning())
        pleaseReOpen();
}

void QnVMax480LiveProvider::updateStreamParamsBasedOnFps()
{
    if (isRunning())
        pleaseReOpen();
}

int QnVMax480LiveProvider::getChannel() const
{
    return m_resource.dynamicCast<QnPhysicalCameraResource>()->getChannel();
}
