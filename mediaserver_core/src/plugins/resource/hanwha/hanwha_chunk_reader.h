#pragma once

#if defined(ENABLE_HANWHA)

#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <nx/network/buffer.h>
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
        loadingOverlappedIds,
        loadingChunks,
    };

    struct Parameter
    {
        Parameter(const QString& name, const QString& value):
            name(name),
            value(value)
        {
        };

        QString name;
        QString value;
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

    boost::optional<int> overlappedId() const;

    void setEnableUtcTime(bool enableUtcTime);
    void setEnableSearchRecordingPeriodRetieval(bool enableRetrieval);

    QString convertDateToString(const QDateTime& dateTime) const;

    bool hasBounds() const;

private:
    void sendRequest();
    void sendOverlappedIdRequest();
    void sendTimelineRequest();
    void sendTimeRangeRequest();

    void handleSuccessfulTimeRangeResponse();
    void handleSuccessfulOverlappedIdResponse();
    void handlerSuccessfulChunksResponse();

    // Returns true if an HTTP error occurred.
    bool handleHttpError();

    boost::optional<Parameter> parseLine(const nx::Buffer& line) const;
    void parseTimeRangeData(const nx::Buffer& data);
    bool parseTimelineData(const nx::Buffer& data, qint64 currentTimeMs);
    void parseOverlappedIdListData(const nx::Buffer& data);

    void prepareHttpClient();
    void scheduleNextRequest(const std::chrono::milliseconds& delay);
    qint64 latestChunkTimeMs() const;

    QnTimePeriodList chunksThreadUnsafe(int channelNumber) const;
    void setError();
    State nextState(State currentState) const;
    QString makeChannelIdListString() const;
    QString makeStartDateTimeString() const;
    QString makeEndDateTimeSting() const;

    void at_httpClientDone();
    void at_gotChunkData();
private:
    std::unique_ptr<nx_http::AsyncClient> m_httpClient;
    State m_state = State::initial;
    nx::Buffer m_unfinishedLine;
    std::vector<QnTimePeriodList> m_chunks;
    std::vector<QnTimePeriodList> m_newChunks;
    qint64 m_nextRequestTimerId = 0;

    qint64 m_startTimeUs = AV_NOPTS_VALUE;
    qint64 m_endTimeUs = AV_NOPTS_VALUE;
    qint64 m_lastParsedStartTimeMs = AV_NOPTS_VALUE;
    boost::optional<int> m_overlappedId;

    int m_maxChannels = 0;
    HanwhaSharedResourceContext* m_resourceContext = nullptr;
    mutable QnMutex m_mutex;

    std::atomic<std::chrono::seconds> m_timeZoneShift{std::chrono::seconds(0)};
    std::atomic<bool> m_terminated{false};

    std::atomic<bool> m_timelineLoadedAtLeastOnce{false};
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
