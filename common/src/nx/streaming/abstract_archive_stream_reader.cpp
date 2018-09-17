#include "abstract_archive_stream_reader.h"

#include <nx/utils/log/log.h>
#include <utils/common/util.h>

#include <recording/time_period.h>
#include <recording/time_period_list.h>
#include "core/resource/media_resource.h"
#include "utils/common/sleep.h"
#include <nx/streaming/video_data_packet.h>
#include "abstract_archive_integrity_watcher.h"

// TODO: #rvasilenko filters are moved to mediaserver core
//#include <core/dataprovider/abstract_media_data_filter.h>

QnAbstractArchiveStreamReader::QnAbstractArchiveStreamReader(const QnResourcePtr &dev):
    QnAbstractMediaStreamDataProvider(dev)
{
}

QnAbstractArchiveStreamReader::~QnAbstractArchiveStreamReader()
{
    stop();
    delete m_delegate;
}

QnAbstractNavigator *QnAbstractArchiveStreamReader::navDelegate() const
{
    return m_navDelegate;
}

// ------------------- Audio tracks -------------------------

unsigned QnAbstractArchiveStreamReader::getCurrentAudioChannel() const
{
    return 0;
}

CameraDiagnostics::Result QnAbstractArchiveStreamReader::diagnoseMediaStreamConnection()
{
    //TODO/IMPL
    return CameraDiagnostics::Result(CameraDiagnostics::ErrorCode::unknown );
}

QStringList QnAbstractArchiveStreamReader::getAudioTracksInfo() const
{
    return QStringList();
}

bool QnAbstractArchiveStreamReader::setAudioChannel(unsigned /*num*/)
{
    return false;
}

void QnAbstractArchiveStreamReader::setNavDelegate(QnAbstractNavigator* navDelegate)
{
    m_navDelegate = navDelegate;
}

QnAbstractArchiveDelegate* QnAbstractArchiveStreamReader::getArchiveDelegate() const
{
    return m_delegate;
}

void QnAbstractArchiveStreamReader::setCycleMode(bool value)
{
    m_cycleMode = value;
}

bool QnAbstractArchiveStreamReader::open(AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher)
{
    m_archiveIntegrityWatcher = archiveIntegrityWatcher;
    return m_delegate && m_delegate->open(m_resource, archiveIntegrityWatcher);
}

void QnAbstractArchiveStreamReader::jumpToPreviousFrame(qint64 usec)
{
    if (usec != DATETIME_NOW)
        jumpTo(qMax(0ll, usec - 200 * 1000), usec-1);
    else
        jumpTo(usec, 0);
}

quint64 QnAbstractArchiveStreamReader::lengthUsec() const
{
    if (m_delegate)
        return m_delegate->endTime() - m_delegate->startTime();
    else
        return AV_NOPTS_VALUE;
}

void QnAbstractArchiveStreamReader::addMediaFilter(const std::shared_ptr<AbstractMediaDataFilter>& filter)
{
    m_filters.push_back(filter);
}

void QnAbstractArchiveStreamReader::run()
{
    initSystemThreadId();

    NX_VERBOSE(this, "Started");
    beforeRun();

    while(!needToStop())
    {
        pauseDelay(); // pause if needed;
        if (needToStop()) // extra check after pause
            break;

        // check queue sizes

        if (!dataCanBeAccepted())
        {
            emit waitForDataCanBeAccepted();
            QnSleep::msleep(10);
            continue;
        }

        QnAbstractMediaDataPtr data = getNextData();

//        for (const auto& filter: m_filters)
//            data = std::dynamic_pointer_cast<QnAbstractMediaData>(filter->processData(data));

        if (!data && !needToStop())
        {
            if (m_noDataHandler)
                m_noDataHandler();

            setNeedKeyData();
            m_stat[0].onEvent(CameraDiagnostics::BadMediaStreamResult());

            QnSleep::msleep(30);
            continue;
        }

        checkTime(data);

        QnCompressedVideoDataPtr videoData = std::dynamic_pointer_cast<QnCompressedVideoData>(data);

        if (videoData && videoData->channelNumber>CL_MAX_CHANNEL_NUMBER-1)
        {
            NX_ASSERT(false);
            continue;
        }

        if (videoData && needKeyData(videoData->channelNumber))
        {
            if (videoData->flags & AV_PKT_FLAG_KEY)
                m_gotKeyFrame[videoData->channelNumber]++;
            else
                continue; // need key data but got not key data
        }

        if(data)
            data->dataProvider = this;

        auto mediaRes = m_resource.dynamicCast<QnMediaResource>();
        if (mediaRes && !mediaRes->hasVideo(this))
        {
            if (data)
                m_stat[data->channelNumber].onData(
                    static_cast<unsigned int>(data->dataSize()), false);
        }
        else {
            if (videoData)
                m_stat[data->channelNumber].onData(
                    static_cast<unsigned int>(data->dataSize()),
                    videoData->flags & AV_PKT_FLAG_KEY);
        }


        putData(std::move(data));
    }

    afterRun();
    NX_VERBOSE(this, "Stopped");
}

void QnAbstractArchiveStreamReader::setNoDataHandler(
    nx::utils::MoveOnlyFunc<void()> noDataHandler)
{
    m_noDataHandler = std::move(noDataHandler);
}
