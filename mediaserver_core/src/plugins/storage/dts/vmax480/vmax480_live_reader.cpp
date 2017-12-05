#ifdef ENABLE_VMAX

#include "vmax480_live_reader.h"

extern "C"
{
    #include <libavcodec/avcodec.h>
}

#include <QtCore/QUrlQuery>

#include "core/resource/network_resource.h"
#include "nx/streaming/media_data_packet.h"
#include "utils/common/sleep.h"
#include "utils/common/synctime.h"

#include "vmax480_resource.h"
#include "utils/common/util.h"



static const QByteArray GROUP_ID("{347E1C92-4627-405d-99B3-5C7EF78B0055}");

// ----------------------------------- QnVMax480LiveProvider -----------------------

QnVMax480LiveProvider::QnVMax480LiveProvider(const QnResourcePtr& dev ):
    CLServerPushStreamReader(dev),
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
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    if (needMetaData())
        return getMetaData();

    QnAbstractDataPacketPtr result;
    QElapsedTimer getTimer;
    getTimer.restart();
    while (!needToStop() && isStreamOpened() && getTimer.elapsed() < MAX_FRAME_DURATION_MS * 2 && !result)
    {
        result = m_maxStream->getNextData(this);
    }

    if (!result)
        closeStream();

    if (!m_needStop) 
    {
        QnAbstractMediaDataPtr media = std::dynamic_pointer_cast<QnAbstractMediaData>(result);
        if (media && (media->dataType != QnAbstractMediaData::EMPTY_DATA)) {
            m_lastMediaTimer.restart();
            if (getResource()->getStatus() == Qn::Unauthorized || getResource()->getStatus() == Qn::Offline)
                getResource()->setStatus(Qn::Online);
        }
        else if (m_lastMediaTimer.elapsed() > MAX_FRAME_DURATION_MS * 2) {
            m_resource->setStatus(Qn::Offline);
        }
    }

    return std::dynamic_pointer_cast<QnAbstractMediaData>(result);
}

bool QnVMax480LiveProvider::canChangeStatus() const
{
    return false; // do not allow to ancessor update status
}


CameraDiagnostics::Result QnVMax480LiveProvider::openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params)
{
    Q_UNUSED(isCameraControlRequired);
    Q_UNUSED(params)
    if (m_opened)
        return CameraDiagnostics::NoErrorResult();

    int channel = QUrlQuery(QUrl(m_resource->getUrl()).query()).queryItemValue(QLatin1String("channel")).toInt();
    if (channel > 0)
        channel--;


    if (m_maxStream == 0)
        m_maxStream = VMaxStreamFetcher::getInstance(GROUP_ID, m_resource.data(), true);
    m_opened = m_maxStream->registerConsumer(this); 
    m_lastMediaTimer.restart();

    return CameraDiagnostics::NoErrorResult();
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
        m_maxStream->freeInstance(GROUP_ID, m_resource.data(), true);
    m_maxStream = 0;

    //vmaxDisconnect();
}

bool QnVMax480LiveProvider::isStreamOpened() const
{
    return m_opened;
}

void QnVMax480LiveProvider::beforeRun()
{
    CLServerPushStreamReader::beforeRun();
    //msleep(300);
}

void QnVMax480LiveProvider::afterRun()
{
    //msleep(300);
    CLServerPushStreamReader::afterRun();
    closeStream();
}

int QnVMax480LiveProvider::getChannel() const
{
    return m_resource.dynamicCast<QnPhysicalCameraResource>()->getChannel();
}

#endif // #ifdef ENABLE_VMAX
