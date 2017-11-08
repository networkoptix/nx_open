#pragma once

#if defined(ENABLE_HANWHA)

#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <nx/network/http/http_async_client.h>
#include <recording/time_period_list.h>

extern "C"
{
// For const AV_NOPTS_VALUE.
#include <libavutil/avutil.h>
}

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaSharedResourceContext;

class HanwhaChunkLoader: public QObject
{
    Q_OBJECT

    enum class State
    {
        initial,
        updateTimeRange,
        loadingChunks
    };

public:
    HanwhaChunkLoader();
    virtual ~HanwhaChunkLoader();

    void start(HanwhaSharedResourceContext* resourceContext);
    bool isStarted() const;

    qint64 startTimeUsec(int channelNumber) const;
    qint64 endTimeUsec(int channelNumber) const;
    QnTimePeriodList chunks(int channelNumber) const;
    QnTimePeriodList chunksSync(int channelNumber) const;

    void setTimeZoneShift(std::chrono::seconds timeZoneShift);
    std::chrono::seconds timeZoneShift() const;

    void setEnableUtcTime(bool enableUtcTime);
    void setEnableSearchRecordingPeriodRetieval(bool enableRetrieval);

    QString convertDateToString(const QDateTime& dateTime) const;

    bool hasBounds() const;

signals:
    void gotChunks();

private:
    QnTimePeriodList chunksUnsafe(int channelNumber) const;
    void setError();
    State nextState(State currentState) const;

    void onHttpClientDone();
    void onGotChunkData();
    bool parseChunkData(const QByteArray& data, qint64 currentTimeMs);

    void sendLoadChunksRequest();
    void sendUpdateTimeRangeRequest();
    void startTimerForNextRequest(const std::chrono::milliseconds& delay);
    void sendRequest();
    void parseTimeRangeData(const QByteArray& data);
    qint64 latestChunkTimeMs() const;
    QUrl buildUrl(const QString& path, std::map<QString, QString> parameters) const;
    void prepareHttpClient();

private:
    std::unique_ptr<nx_http::AsyncClient> m_httpClient;
    State m_state = State::initial;
    QByteArray m_unfinishedLine;
    std::vector<QnTimePeriodList> m_chunks;
    std::vector<QnTimePeriodList> m_newChunks;
    qint64 m_nextRequestTimerId = 0;

    qint64 m_startTimeUsec = AV_NOPTS_VALUE;
    qint64 m_endTimeUsec = AV_NOPTS_VALUE;
    qint64 m_lastParsedStartTimeMs = AV_NOPTS_VALUE;

    int m_maxChannels = 0;
    HanwhaSharedResourceContext* m_resourceContext = nullptr;
    mutable QnMutex m_mutex;

    std::atomic<std::chrono::seconds> m_timeZoneShift{std::chrono::seconds(0)};
    std::atomic<bool> m_terminated{false};

    std::atomic<bool> m_chunksLoadedAtLeastOnce{false};
    std::atomic<bool> m_timeRangeLoadedAtLeastOnce{false};
    mutable std::atomic<bool> m_errorOccured{false};
    mutable QnWaitCondition m_wait;

    bool m_isNvr = false;
    bool m_hasSearchRecordingPeriodSubmenu = false;
    bool m_isSearchRecordingPeriodRetrievalEnabled = true;
    bool m_isUtcEnabled = true;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
