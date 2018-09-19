#pragma once

#include <nx/network/http/http_async_client.h>
#include <nx/streaming/media_data_packet.h>

class ArecontMetaReader
{
public:
    struct MdParsingInfo
    {
        int totalMdZones;
        int zoneSize;
        int maxSensorWidth;
        int maxSensorHight;
    };

public:
    ArecontMetaReader(int channelCount);
    ~ArecontMetaReader();

    void asyncRequest(const QString& host, const QAuthenticator& auth, const MdParsingInfo& info);
    bool busy() { return m_metaDataClientBusy; }
    bool hasData();
    QnMetaDataV1Ptr getData();

private:
    void onMetaData(const MdParsingInfo& info);

private:
    nx_http::AsyncClient m_metaDataClient;
    QnMutex m_metaDataMutex;
    std::atomic<bool> m_metaDataClientBusy = false;
    const int m_channelCount;
    int m_currentChannel = 0;
    QnMetaDataV1Ptr m_metaData;
};
