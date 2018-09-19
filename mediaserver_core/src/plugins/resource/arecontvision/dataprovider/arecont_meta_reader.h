#pragma once

#include <chrono>
#include <nx/network/http/http_async_client.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/utils/elapsed_timer.h>

#include "../resource/av_resource.h"


struct MdParsingInfo;
class ArecontMetaReader
{
public:
    ArecontMetaReader(
        int channelCount, std::chrono::milliseconds minRepeatInterval, int minFrameInterval);
    ~ArecontMetaReader();

    void requestIfReady(QnPlAreconVisionResource* resource);
    bool hasData();
    QnMetaDataV1Ptr getData();
    void onNewFrame() { ++m_framesSinceLastMetaData; }

private:
    void asyncRequest(QnPlAreconVisionResource* resource);
    void onMetaData(const MdParsingInfo& info);

private:
    nx_http::AsyncClient m_metaDataClient;
    QnMutex m_metaDataMutex;
    std::atomic<bool> m_metaDataClientBusy = false;
    const int m_channelCount;
    int m_currentChannel = 0;
    QnMetaDataV1Ptr m_metaData;

    const std::chrono::milliseconds m_minRepeatInterval;
    const int m_minFrameInterval;
    nx::utils::ElapsedTimer m_lastMetaRequest;
    int m_framesSinceLastMetaData = 0;
};
