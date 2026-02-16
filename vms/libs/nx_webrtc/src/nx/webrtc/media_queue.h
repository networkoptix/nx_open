// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <vector>

#include <nx/media/media_data_packet.h>
#include <nx/utils/thread/mutex.h>

namespace nx::vms::server {

class NX_WEBRTC_API MediaQueue
{
public:
    MediaQueue(int maxSize);
    void putData(const QnConstAbstractMediaDataPtr& data);
    QnConstAbstractMediaDataPtr popData(bool blocked = false);
    void stop();
    void setMaxSize(int maxSize);
    void clearUnprocessedData();

    int size() const;

private:
    std::chrono::microseconds getDurationUnsafe();
    void clearTillLastGopUnsafe();

private:
    int m_maxSize;
    std::deque<QnConstAbstractMediaDataPtr> m_mediaQueue;
    std::vector<bool> m_keyDataFound;

    nx::WaitCondition m_waitForFrame;
    mutable nx::Mutex m_mutex;
    std::atomic<bool> m_needToStop = false;
};

} // namespace nx::vms::server
