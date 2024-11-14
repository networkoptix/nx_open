// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "thumbnails_stream_reader.h"

#include <core/resource/avi/avi_archive_delegate.h>
#include <core/resource/avi/thumbnails_archive_delegate.h>
#include <core/resource/camera_resource.h>
#include <nx/utils/log/log.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>

// used in reverse mode.
// seek by 1.5secs. It is prevents too fast seeks for short GOP, also some codecs has bagged seek function. Large step prevent seek
// forward instead seek backward
//static const int MAX_KEY_FIND_INTERVAL = 10'000'000;

//static const int FFMPEG_PROBE_BUFFER_SIZE = 512 * 1024;
//static const qint64 LIVE_SEEK_OFFSET = 10'000'000ll;

QnThumbnailsStreamReader::QnThumbnailsStreamReader(
    const QnResourcePtr& resource,
    QnAbstractArchiveDelegate* archiveDelegate)
    :
    QnAbstractMediaStreamDataProvider(resource),
    m_archiveDelegate(archiveDelegate),
    m_delegate(new QnThumbnailsArchiveDelegate(QnAbstractArchiveDelegatePtr(archiveDelegate)))
{
    m_archiveDelegate->setQuality(MEDIA_Quality_Low, true, QSize());
}

void QnThumbnailsStreamReader::setQuality(MediaQuality q)
{
    m_archiveDelegate->setQuality(q, true, QSize());
}

QnThumbnailsStreamReader::~QnThumbnailsStreamReader()
{
    stop();
    delete m_delegate;
}

void QnThumbnailsStreamReader::setRange(qint64 startTime, qint64 endTime, qint64 frameStep, int cseq)
{
    m_delegate->setRange(startTime, endTime, frameStep);
    m_cseq = cseq;
}

QnAbstractMediaDataPtr QnThumbnailsStreamReader::createEmptyPacket()
{
    QnAbstractMediaDataPtr rez(new QnEmptyMediaData());
    rez->timestamp = DATETIME_NOW;
    rez->opaque = m_cseq;
    msleep(50);
    return rez;
}

QnAbstractMediaDataPtr QnThumbnailsStreamReader::getNextData()
{
    QnAbstractMediaDataPtr result = m_delegate->getNextData();
    if (result)
        return result;
    else
        return createEmptyPacket();
}

void QnThumbnailsStreamReader::run()
{
    NX_INFO(this, "Started");
    beforeRun();

    const bool isOpened = m_delegate->open(getResource());
    if (!isOpened)
        NX_WARNING(this, "Unable to open stream for resource %1", getResource()->getId());
    while(isOpened && !needToStop())
    {
        pauseDelay(); // pause if needed;
        if (needToStop()) // extra check after pause
            break;

        // check queue sizes

        if (!dataCanBeAccepted())
        {
            msleep(5);
            continue;
        }

        QnAbstractMediaDataPtr data = getNextData();

        if (data==0 && !m_needStop)
        {
            setNeedKeyData();
            onEvent(CameraDiagnostics::BadMediaStreamResult());
            msleep(30);
            continue;
        }

        QnCompressedVideoDataPtr videoData = std::dynamic_pointer_cast<QnCompressedVideoData>(data);

        if (videoData && videoData->channelNumber>CL_MAX_CHANNEL_NUMBER-1)
        {
            NX_ASSERT(false);
            continue;
        }

        if (videoData && needKeyData())
        {
            if (videoData->flags & AV_PKT_FLAG_KEY)
                m_gotKeyFrame.at(videoData->channelNumber)++;
            else
                continue; // need key data but got not key data
        }

        if(data) {
            data->dataProvider = this;
            data->opaque = m_cseq;
        }

        if (videoData)
            m_stat[videoData->channelNumber].onData(videoData);

        putData(data);
    }

    afterRun();
    NX_INFO(this, "Stopped");
}

void QnThumbnailsStreamReader::afterRun()
{
    if (m_delegate)
        m_delegate->close();
}

void QnThumbnailsStreamReader::setGroupId(const nx::String& groupId)
{
    m_delegate->setGroupId(groupId);
}
