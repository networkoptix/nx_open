#include "abstract_media_stream_data_provider.h"

#include <core/resource/resource_media_layout.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/config.h>
#include <utils/common/sleep.h>
#include <utils/common/util.h>
#include <nx/utils/log/log.h>
#include <core/resource/camera_resource.h>
#include <nx/streaming/nx_streaming_ini.h>
#include <utils/common/synctime.h>
#include <nx/utils/time_helper.h>

static const qint64 TIME_RESYNC_THRESHOLD = 1000000ll * 15;

QnAbstractMediaStreamDataProvider::QnAbstractMediaStreamDataProvider(const QnResourcePtr& res)
    :
    QnAbstractStreamDataProvider(res),
    m_numberOfchannels(0)
{
    memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame));
    m_mediaResource = res;
    NX_ASSERT(dynamic_cast<QnMediaResource*>(m_mediaResource.data()));
    m_isCamera = dynamic_cast<const QnSecurityCamResource*>(res.data()) != nullptr;

    connect(
        &m_stat[0], &QnMediaStreamStatistics::streamEvent,
        this, [this](CameraDiagnostics::Result result)
        {
            emit streamEvent(this, result);
        }, Qt::DirectConnection);
}

QnAbstractMediaStreamDataProvider::~QnAbstractMediaStreamDataProvider()
{
    stop();
}

void QnAbstractMediaStreamDataProvider::setNeedKeyData(int channel)
{
    QnMutexLocker mtx( &m_mutex );

    loadNumberOfChannelsIfUndetected();

    if (m_numberOfchannels < CL_MAX_CHANNEL_NUMBER && channel < m_numberOfchannels)
        m_gotKeyFrame[channel] = 0;
}

void QnAbstractMediaStreamDataProvider::loadNumberOfChannelsIfUndetected() const
{
    if (m_numberOfchannels != 0)
        return;
    if (auto mediaRes = dynamic_cast<QnMediaResource*>(m_mediaResource.data()))
        m_numberOfchannels = mediaRes->getVideoLayout(this)->channelCount();
}

void QnAbstractMediaStreamDataProvider::setNeedKeyData()
{
    QnMutexLocker mtx( &m_mutex );

    loadNumberOfChannelsIfUndetected();

    for (int i = 0; i < m_numberOfchannels; ++i)
        m_gotKeyFrame[i] = 0;
}

bool QnAbstractMediaStreamDataProvider::needKeyData(int channel) const
{
    QnMutexLocker mtx( &m_mutex );
    return m_gotKeyFrame[channel]==0;
}

bool QnAbstractMediaStreamDataProvider::needKeyData() const
{
    QnMutexLocker mtx( &m_mutex );

    loadNumberOfChannelsIfUndetected();

    for (int i = 0; i < m_numberOfchannels; ++i)
        if (m_gotKeyFrame[i]==0)
            return true;

    return false;
}


void QnAbstractMediaStreamDataProvider::beforeRun()
{
    setNeedKeyData();
    for (int i = 0; i < CL_MAX_CHANNEL_NUMBER; ++i)
        m_stat[i].resetStatistics();
}

void QnAbstractMediaStreamDataProvider::afterRun()
{
}

const QnMediaStreamStatistics* QnAbstractMediaStreamDataProvider::getStatistics(int channel) const
{
    return &m_stat[channel];
}

int QnAbstractMediaStreamDataProvider::getNumberOfChannels() const
{
    NX_ASSERT(m_numberOfchannels, Q_FUNC_INFO, "No channels?");
    return m_numberOfchannels ? m_numberOfchannels : 1;
}

float QnAbstractMediaStreamDataProvider::getBitrateMbps() const
{
    float rez = 0;
    for (int i = 0; i < m_numberOfchannels; ++i)
        rez += m_stat[i].getBitrateMbps();
    return rez;
}

float QnAbstractMediaStreamDataProvider::getFrameRate() const
{
    float rez = 0;
    for (int i = 0; i < m_numberOfchannels; ++i)
        rez += m_stat[i].getFrameRate();

    return rez / getNumberOfChannels();
}

float QnAbstractMediaStreamDataProvider::getAverageGopSize() const
{
    float rez = 0;
    for (int i = 0; i < m_numberOfchannels; ++i)
        rez += m_stat[i].getAverageGopSize();

    return rez / getNumberOfChannels();
}

QnConstMediaContextPtr QnAbstractMediaStreamDataProvider::getCodecContext() const
{
    return QnConstMediaContextPtr(nullptr);
}

CameraDiagnostics::Result QnAbstractMediaStreamDataProvider::diagnoseMediaStreamConnection()
{
    return CameraDiagnostics::NotImplementedResult();
}
