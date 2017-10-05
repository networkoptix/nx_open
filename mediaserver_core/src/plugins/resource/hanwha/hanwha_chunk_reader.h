#pragma once

#if defined(ENABLE_HANWHA)

#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <nx/network/http/http_async_client.h>
#include <recording/time_period_list.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaChunkLoader: public QObject
{
    Q_OBJECT

    enum class State
    {
        Initial,
        updateTimeRange,
        LoadingChunks
    };

public:
    HanwhaChunkLoader();
    virtual ~HanwhaChunkLoader();

    void start(const QAuthenticator& auth, const QUrl& url, int channelCount);
    bool isStarted() const;

    qint64 startTimeUsec(int channelNumber) const;
    qint64 endTimeUsec(int channelNumber) const;
    QnTimePeriodList chunks(int channelNumber) const;
private:
    void onHttpClientDone();
    void onGotChunkData();
    bool parseChunkData(const QByteArray& data);

    void sendLoadChunksRequest();
    void sendUpdateTimeRangeRequest();
    void startTimerForNextRequest(const std::chrono::milliseconds& delay);
    void sendRequest();
    void parseTimeRangeData(const QByteArray& data);
    qint64 latestChunkTime() const;
private:
    std::unique_ptr<nx_http::AsyncClient> m_httpClient;
    State m_state = State::Initial;
    QByteArray m_unfinishedLine;
    std::vector<QnTimePeriodList> m_chunks;
    qint64 m_nextRequestTimerId = 0;

    qint64 m_startTimeUsec = AV_NOPTS_VALUE;
    qint64 m_endTimeUsec = AV_NOPTS_VALUE;
    qint64 m_lastParsedStartTime = AV_NOPTS_VALUE;

    int m_maxChannels = 0;
    QAuthenticator m_auth;
    QUrl m_cameraUrl;
    mutable QnMutex m_mutex;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
