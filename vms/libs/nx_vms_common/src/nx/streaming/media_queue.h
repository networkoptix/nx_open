// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <vector>

#include <nx/media/media_data_packet.h>

class NX_VMS_COMMON_API MediaQueue
{
public:
    void putData(const QnConstAbstractMediaDataPtr& data);
    QnConstAbstractMediaDataPtr popData();
    std::chrono::microseconds getDurationUnsafe();
    void clearUnprocessedDataUnsafe();
    void clearTillLastGopUnsafe();
    int size() const;
    bool empty() const { return size() == 0; }

private:
    std::deque<QnConstAbstractMediaDataPtr> m_mediaQueue;
    std::vector<bool> m_keyDataFound;
};
