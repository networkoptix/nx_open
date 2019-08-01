#pragma once

#ifdef ENABLE_ARECONT

#include <chrono>
#include <nx/network/http/http_async_client.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/utils/elapsed_timer.h>

#include "../resource/av_resource.h"

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
    struct ParsingInfo
    {
        int totalMdZones = 0;
        int zoneSize = 0;
        int maxSensorWidth = 0;
        int maxSensorHight = 0;
    };
    void requestAsync(QnPlAreconVisionResource* resource);
    void onMetaData(const ParsingInfo& info);
    static QnMetaDataV1Ptr parseMotionMetadata(
        int channel, const QString& response, const ParsingInfo& info);

private:
    nx::network::http::AsyncHttpClientPtr m_metaDataClient;
    QnMutex m_metaDataMutex;
    std::atomic<bool> m_metaDataClientBusy = false;
    const int m_channelCount = 0;
    int m_currentChannel = 0;
    QnMetaDataV1Ptr m_metaData;

    const std::chrono::milliseconds m_minRepeatInterval;
    const int m_minFrameInterval = 0;
    nx::utils::ElapsedTimer m_lastMetaRequest;
    std::atomic<int> m_framesSinceLastMetaData = 0;
};

#endif // ENABLE_ARECONT
