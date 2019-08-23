#pragma once

#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <nx/network/buffer.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/timer_manager.h>

#include <core/resource/abstract_remote_archive_manager.h>
#include <recording/time_period_list.h>

#include <plugins/resource/hanwha/hanwha_information.h>

extern "C" {

// For const AV_NOPTS_VALUE.
#include <libavutil/avutil.h>

} // extern "C"

namespace nx {
namespace vms::server {
namespace plugins {

class HanwhaSharedResourceContext;

struct HanwhaChunkLoaderSettings
{
    HanwhaChunkLoaderSettings() = default;
    HanwhaChunkLoaderSettings(
        const std::chrono::seconds& responseTimeout,
        const std::chrono::seconds& messageBodyReadTimeout);

    std::chrono::seconds responseTimeout = std::chrono::minutes(5);
    std::chrono::seconds messageBodyReadTimeout = std::chrono::minutes(30);
};

class HanwhaChunkLoader: public QObject
{
    Q_OBJECT

    enum class State
    {
        initial,
        loadingRecordingPeriod,
        loadingOverlappedIds,
        loadingTimeline,
    };

    struct Parameter
    {
        Parameter(const QString& name, const QString& value):
            name(name),
            value(value)
        {
        }

        QString name;
        QString value;
    };

    using OverlappedId = int;
    using OverlappedIdList = std::vector<OverlappedId>;
    using ChunksByChannel = std::vector<QnTimePeriodList>;
    using OverlappedChunks = std::map<int, ChunksByChannel>;

public:
    HanwhaChunkLoader(
        HanwhaSharedResourceContext* resourceContext,
        const HanwhaChunkLoaderSettings& settings);

    virtual ~HanwhaChunkLoader();

    void start(const HanwhaInformation& information);
    bool isStarted() const;

    qint64 startTimeUsec(int channelNumber) const;
    qint64 endTimeUsec(int channelNumber) const;
    nx::core::resource::OverlappedTimePeriods overlappedTimeline(int channelNumber) const;

    /**
     * This method MUST not be called after start().
     */
    nx::core::resource::OverlappedTimePeriods overlappedTimelineSync(int channelNumber);

    std::chrono::milliseconds timeShift() const;
    void setTimeShift(std::chrono::milliseconds value);

    /**
     * Returns a default overlapped ID for a device (the highest one).
     * This method should be used only for NVRs since cameras doesn't have such
     * kind of "default" overlapped ID and all overlapped periods
     * should be properly handled for them.
     */
    boost::optional<int> overlappedId() const;

    void setEnableSearchRecordingPeriodRetieval(bool enableRetrieval);

    QString convertDateToString(const QDateTime& dateTime) const;

    bool hasBounds() const;

private:
    void sendRequest();
    void sendRecordingPeriodRequest();
    void sendOverlappedIdRequest();
    void sendTimelineRequest();

    void handleSuccessfulRecordingPeriodResponse();
    void handleSuccessfulOverlappedIdResponse();
    void handleSuccessfulTimelineResponse();

    // Returns true if an HTTP error occurred.
    bool handleHttpError();

    boost::optional<Parameter> parseLine(const nx::Buffer& line) const;
    void parseTimeRangeData(const nx::Buffer& data);
    bool parseTimelineData(const nx::Buffer& data, qint64 currentTimeMs);
    void parseOverlappedIdListData(const nx::Buffer& data);

    void prepareHttpClient();
    void scheduleNextRequest(const std::chrono::milliseconds& delay);
    qint64 latestChunkTimeMs() const;
    void sortTimeline(OverlappedChunks* outTimeline) const;

    nx::core::resource::OverlappedTimePeriods overlappedTimelineThreadUnsafe(
        int channelNumber) const;

    void setError();
    State nextState(State currentState) const;
    QString makeChannelIdListString() const;
    QString makeStartDateTimeString() const;
    QString makeEndDateTimeSting() const;

    std::chrono::milliseconds timeSinceLastTimelineUpdate() const;

    void setUpThreadUnsafe(const HanwhaInformation& information);

    void at_httpClientDone();
    void at_gotChunkData();

    bool isEdge() const;
    bool isNvr() const;

private:

    std::unique_ptr<nx::network::http::AsyncClient> m_httpClient;
    State m_state = State::initial;
    nx::Buffer m_unfinishedLine;

    OverlappedChunks m_chunks;
    OverlappedChunks m_newChunks;
    nx::utils::TimerId m_nextRequestTimerId = 0;

    qint64 m_startTimeUs = AV_NOPTS_VALUE;
    qint64 m_endTimeUs = AV_NOPTS_VALUE;
    qint64 m_lastParsedStartTimeMs = AV_NOPTS_VALUE;
    qint64 m_lastParsedEndTimeMs = AV_NOPTS_VALUE;

    OverlappedId m_lastNvrOverlappedId = -1;
    OverlappedIdList m_overlappedIds;
    OverlappedIdList::const_iterator m_currentOverlappedId;

    int m_maxChannels = 0;
    HanwhaSharedResourceContext* m_resourceContext = nullptr;
    mutable QnMutex m_mutex;

    std::atomic<std::chrono::milliseconds> m_timeShift{std::chrono::milliseconds(0)};
    std::atomic<bool> m_terminated{false};

    std::atomic<bool> m_started{false};
    mutable std::atomic<bool> m_errorOccured{false};
    mutable std::atomic<bool> m_isInProgress{false};
    mutable QnWaitCondition m_wait;

    bool m_isNvr = false;
    bool m_hasSearchRecordingPeriodSubmenu = false;
    bool m_isSearchRecordingPeriodRetrievalEnabled = true;

    std::chrono::milliseconds m_lastTimelineUpdate{0};

    HanwhaChunkLoaderSettings m_settings;
};

} // namespace plugins
} // namespace vms::server
} // namespace nx
