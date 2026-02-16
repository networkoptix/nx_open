// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_queue.h"

#include <nx/utils/log/log.h>
#include <nx/media/video_data_packet.h>

namespace nx::vms::server {

MediaQueue::MediaQueue(int maxSize):
    m_maxSize(maxSize)
{
}

// Normally should drop only first gop for the first appeared channel in media queue, and all audio
// before. If there is no keyframes in queue clear queue().
void MediaQueue::clearTillLastGopUnsafe()
{
    NX_DEBUG(this, "Clear media queue due to overflow, size: %1", m_mediaQueue.size());

    for (auto media = m_mediaQueue.begin(); media != m_mediaQueue.end(); ++media)
    {
        bool isKeyFrame = false;
        if (const auto* video = dynamic_cast<const QnCompressedVideoData*>(media->get()))
            isKeyFrame = video->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey);

        if (isKeyFrame)
        {
            auto deviceId = (*media)->deviceId;
            auto channelNumber = (*media)->channelNumber;
            NX_DEBUG(this, "Clear GOP for device: %1, channel: %2", deviceId, channelNumber);
            media->get()->flags |= QnAbstractMediaData::MediaFlags_Discontinuity;
            bool removed = false;
            auto erasedVideoStart = std::remove_if(
                m_mediaQueue.begin(),
                media,
                [&channelNumber, deviceId, &removed](const auto& value)
                {
                    if (const auto* video =
                        dynamic_cast<const QnCompressedVideoData*>(value.get()))
                    {
                        if (video->channelNumber == channelNumber && video->deviceId == deviceId)
                        {
                            removed = true;
                            return true;
                        }
                    }
                    return false;
                });
            if (removed)
            {
                // Clean not video packets until the same key frame,
                media = m_mediaQueue.erase(erasedVideoStart, media);
                auto erasedAudioStart = std::remove_if(
                    m_mediaQueue.begin(),
                    media,
                    [deviceId](const auto& value)
                    {
                        return value->deviceId == deviceId
                            && value->dataType != QnAbstractMediaData::VIDEO;
                    });
                m_mediaQueue.erase(erasedAudioStart, media);
                // `media` is invalidated at this moment.
                NX_DEBUG(this, "Media queue size after clean: %1", m_mediaQueue.size());
                return;
            }
        }
    }
    // Fallback.
    m_mediaQueue.clear();
    m_keyDataFound.clear();
}

void MediaQueue::setMaxSize(int maxSize)
{
    m_maxSize = maxSize;
}

int MediaQueue::size() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return (int) m_mediaQueue.size();
}

void MediaQueue::putData(const QnConstAbstractMediaDataPtr& media)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if ((int)m_mediaQueue.size() >= m_maxSize)
        clearTillLastGopUnsafe();

    if (const auto& video = std::dynamic_pointer_cast<const QnCompressedVideoData>(media))
    {
        const bool isKeyFrame = video->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey);
        if (m_keyDataFound.size() <= video->channelNumber)
            m_keyDataFound.resize(video->channelNumber + 1);
        if (isKeyFrame)
            m_keyDataFound[video->channelNumber] = true;
        if (!m_keyDataFound[video->channelNumber] && !isKeyFrame)
        {
            NX_VERBOSE(this, "Skip data, waiting for keyframe: %1", media->timestamp);
            return;
        }
    }
    m_mediaQueue.push_back(media);
    m_needToStop = false;
    m_waitForFrame.wakeAll();
}

QnConstAbstractMediaDataPtr MediaQueue::popData(bool blocked)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (blocked)
    {
        while (m_mediaQueue.empty() && !m_needToStop)
            m_waitForFrame.wait(&m_mutex);
    }

    if (m_mediaQueue.empty())
        return nullptr;

    auto media = m_mediaQueue.front();
    m_mediaQueue.pop_front();
    return media;
}

void MediaQueue::stop()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_needToStop = true;
    m_waitForFrame.wakeAll();
}

void MediaQueue::clearUnprocessedData()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_mediaQueue.clear();
    m_keyDataFound.clear();
}

} // namespace nx::vms::server
