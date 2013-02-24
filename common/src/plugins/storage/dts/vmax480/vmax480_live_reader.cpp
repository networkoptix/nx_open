#include "vmax480_live_reader.h"
#include "core/resource/network_resource.h"
#include "core/datapacket/media_data_packet.h"
#include "libavcodec/avcodec.h"
#include "utils/common/sleep.h"
#include "utils/common/synctime.h"

#include "vmax480_resource.h"


// ----------------------------------- QnVMax480LiveProvider -----------------------

QnVMax480LiveProvider::QnVMax480LiveProvider(QnResourcePtr dev ):
    CLServerPushStreamreader(dev),
    VMaxStreamFetcher(dev),
    m_connected(false),
    m_internalQueue(16)
{
    m_networkRes = dev.dynamicCast<QnNetworkResource>();
}

QnVMax480LiveProvider::~QnVMax480LiveProvider()
{
    vmaxDisconnect();
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
    while (!needToStop() && getTimer.elapsed() < MAX_FRAME_DURATION * 2 && !result)
    {
        m_internalQueue.pop(result, 100);
    }

    if (!result)
        closeStream();
    return result.dynamicCast<QnAbstractMediaData>();
}

void QnVMax480LiveProvider::onGotData(QnAbstractMediaDataPtr mediaData)
{
    m_internalQueue.push(mediaData);
}

void QnVMax480LiveProvider::openStream()
{
    if (m_connected)
        return;

    int channel = QUrl(m_res->getUrl()).queryItemValue(QLatin1String("channel")).toInt();
    if (channel > 0)
        channel--;
    m_connected = vmaxConnect(true, channel);
}

void QnVMax480LiveProvider::closeStream()
{
    if (!m_connected)
        return;
    vmaxDisconnect();
    m_connected = false;
}

bool QnVMax480LiveProvider::isStreamOpened() const
{
    return m_connected;
}

void QnVMax480LiveProvider::beforeRun()
{
    msleep(300);
}

void QnVMax480LiveProvider::afterRun()
{
    msleep(300);
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
