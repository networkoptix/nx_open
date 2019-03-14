#include "hanwha_chunk_loader.h"

#include <QtCore/QDateTime>
#include <QtCore/QUrlQuery>

#include <nx/utils/log/log_main.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/scope_guard.h>
#include <utils/common/synctime.h>
#include <core/resource/network_resource.h>

#include "hanwha_request_helper.h"
#include "hanwha_shared_resource_context.h"
#include "hanwha_utils.h"

namespace nx {
namespace vms::server {
namespace plugins {

namespace {

static const int kMaxAllowedChannelNumber = 64;
static const nx::Buffer kStartTimeParamName("StartTime");
static const nx::Buffer kEndTimeParamName("EndTime");
static const nx::Buffer kTypeParamName("Type");
static const QString kOverlappedIdListParameter = lit("OverlappedIDList");

static const std::chrono::seconds kUpdateChunksDelay(60);
static const std::chrono::seconds kResendRequestIfFail(10);

static const QString kDateTimeFormat = lit("yyyy-MM-dd hh:mm:ss");

static const QDateTime kMinDateTime = QDateTime::fromString(
    lit("2000-01-01 00:00:00"),
    kDateTimeFormat);

static const QDateTime kMaxDateTime = QDateTime::fromString(
    lit("2037-12-31 00:00:00"),
    kDateTimeFormat);

static const std::chrono::seconds kSendTimeout(10);
static const std::chrono::milliseconds kTimelineCacheTime(10000); //< Only for sync mode.

} // namespace

HanwhaChunkLoaderSettings::HanwhaChunkLoaderSettings(
    const std::chrono::seconds& responseTimeout,
    const std::chrono::seconds& messageBodyReadTimeout)
    :
    responseTimeout(responseTimeout),
    messageBodyReadTimeout(messageBodyReadTimeout)
{
}

using namespace nx::core::resource;

HanwhaChunkLoader::HanwhaChunkLoader(
    HanwhaSharedResourceContext* resourceContext,
    const HanwhaChunkLoaderSettings& settings)
    :
    m_resourceContext(resourceContext),
    m_settings(settings)
{
}

HanwhaChunkLoader::~HanwhaChunkLoader()
{
    {
        QnMutexLocker lock(&m_mutex);
        m_terminated = true;
    }

    if (m_nextRequestTimerId)
        nx::utils::TimerManager::instance()->joinAndDeleteTimer(m_nextRequestTimerId);

    if (m_httpClient)
        m_httpClient->pleaseStopSync();
}

void HanwhaChunkLoader::start(const HanwhaInformation& information)
{
    QnMutexLocker lock(&m_mutex);
    setUpThreadUnsafe(information);

    if (m_isNvr && !m_started)
    {
        m_state = nextState(m_state);
        m_started = true;
        NX_DEBUG(this, lm("Started for %1 channels on %2")
            .args(m_maxChannels, m_resourceContext->url()));

        sendRequest();
    }
}

bool HanwhaChunkLoader::isStarted() const
{
    QnMutexLocker lock(&m_mutex);
    return m_started;
}

qint64 HanwhaChunkLoader::startTimeUsec(int channelNumber) const
{
    QnMutexLocker lock(&m_mutex);
    auto startTimeUs = std::numeric_limits<qint64>::max();

    for (const auto& entry: m_chunks)
    {
        const auto overlappedId = entry.first;
        const auto& chunksByChannel = entry.second;

        NX_ASSERT(chunksByChannel.size() > channelNumber);
        if (chunksByChannel.size() <= channelNumber || chunksByChannel[channelNumber].isEmpty())
            continue;

        const auto earliestChunkStartTimeUs =
            chunksByChannel[channelNumber].front().startTimeMs * 1000;

        if (earliestChunkStartTimeUs < startTimeUs)
            startTimeUs = earliestChunkStartTimeUs;

        if (m_startTimeUs != AV_NOPTS_VALUE)
            startTimeUs = std::max(startTimeUs, m_startTimeUs);
    }

    if (startTimeUs == std::numeric_limits<int64_t>::max())
        return AV_NOPTS_VALUE;

    return startTimeUs;
}

qint64 HanwhaChunkLoader::endTimeUsec(int channelNumber) const
{
    QnMutexLocker lock(&m_mutex);
    int64_t endTimeUs = std::numeric_limits<int64_t>::max();

    for (const auto& entry: m_chunks)
    {
        const auto overlappedId = entry.first;
        const auto& chunksByChannel = entry.second;

        NX_ASSERT(chunksByChannel.size() > channelNumber);
        if (chunksByChannel.size() <= channelNumber || chunksByChannel[channelNumber].isEmpty())
            continue;

        const auto latestChunkEndTimeUs = chunksByChannel[channelNumber].back().endTimeMs() * 1000;
        if (latestChunkEndTimeUs > endTimeUs)
            endTimeUs = latestChunkEndTimeUs;
    }

    if (endTimeUs == std::numeric_limits<int64_t>::min())
        return AV_NOPTS_VALUE;

    return endTimeUs;
}

std::map<int, QnTimePeriodList> HanwhaChunkLoader::overlappedTimeline(int channelNumber) const
{
    QnMutexLocker lock(&m_mutex);
    return overlappedTimelineThreadUnsafe(channelNumber);
}

OverlappedTimePeriods HanwhaChunkLoader::overlappedTimelineSync(int channelNumber)
{
    QnMutexLocker lock(&m_mutex);

    NX_ASSERT(!m_started);
    if (m_started)
        return OverlappedTimePeriods();

    if (timeSinceLastTimelineUpdate() < kTimelineCacheTime)
        return overlappedTimelineThreadUnsafe(channelNumber);

    if (m_state == State::initial)
    {
        m_state = nextState(State::initial);
        sendRequest();
    }

    while (m_state != State::initial)
        m_wait.wait(&m_mutex);

    return overlappedTimelineThreadUnsafe(channelNumber);
}

std::chrono::milliseconds HanwhaChunkLoader::timeShift() const
{
    return m_timeShift;
}

void HanwhaChunkLoader::setTimeShift(std::chrono::milliseconds value)
{
    // TODO: Find out if it's fine to use for NVR as well.
    if (isEdge() && m_timeShift.load() != value)
    {
        NX_VERBOSE(this, "Device to server time shift has changed to %1", value);
        m_timeShift = value;
    }
}

boost::optional<int> HanwhaChunkLoader::overlappedId() const
{
    QnMutexLocker lock(&m_mutex);
    NX_ASSERT(m_isNvr, lit("Method should be called only for NVRs"));
    if (m_isNvr && !m_overlappedIds.empty())
        return m_overlappedIds.back();

    // For cameras we should import all chunks from all overlapped IDs.
    return boost::none;
}

void HanwhaChunkLoader::setEnableSearchRecordingPeriodRetieval(bool enableRetrieval)
{
    m_isSearchRecordingPeriodRetrievalEnabled = enableRetrieval;
}

QString HanwhaChunkLoader::convertDateToString(const QDateTime& dateTime) const
{
    return dateTime.toString(kHanwhaUtcDateTimeFormat);
}

bool HanwhaChunkLoader::hasBounds() const
{
    return m_hasSearchRecordingPeriodSubmenu
        && m_isSearchRecordingPeriodRetrievalEnabled
        && m_isNvr;
}

//------------------------------------------------------------------------------------------------

void HanwhaChunkLoader::sendRequest()
{
    if (m_terminated)
        return;

    switch (m_state)
    {
        case State::loadingRecordingPeriod:
            sendRecordingPeriodRequest();
            break;
        case State::loadingOverlappedIds:
            sendOverlappedIdRequest();
            break;
        case State::loadingTimeline:
            sendTimelineRequest();
            break;
        default:
            NX_ASSERT(false, lit("Wrong state, should not be here."));
            break;
    }
}

void HanwhaChunkLoader::sendRecordingPeriodRequest()
{
    if (m_terminated)
        return;

    prepareHttpClient();

    // TODO: Check for 'attributes/Recording/Support/SearchPeriod' and use only if it's supported,
    // otherwise we have to load all periods constantly and clean them up by timeout.
    const auto recordingPeriodUrl = HanwhaRequestHelper::buildRequestUrl(
        m_resourceContext,
        lit("recording/searchrecordingperiod/view"),
        {
            {kHanwhaRecordingTypeProperty, kHanwhaAll},
            {kHanwhaResultsInUtcProperty, kHanwhaTrue}
        });

    NX_DEBUG(this, lm("Sending recording period request. Url: %1").arg(recordingPeriodUrl));

    m_httpClient->setOnSomeMessageBodyAvailable(nullptr);
    m_httpClient->doGet(recordingPeriodUrl); //< TODO: Use m_resourceConext->requestSemaphore().
}

void HanwhaChunkLoader::sendOverlappedIdRequest()
{
    if (m_terminated)
        return;

    prepareHttpClient();
    const auto overlappedIdListUrl = HanwhaRequestHelper::buildRequestUrl(
        m_resourceContext,
        lit("recording/overlapped/view"),
        {
            {kHanwhaChannelIdListProperty, makeChannelIdListString()},
            {kHanwhaFromDateProperty, convertDateToString(kMinDateTime)},
            {kHanwhaToDateProperty, makeEndDateTimeSting()}
        });

    NX_DEBUG(this, lm("Sending overlapped ID request. URL: %1").arg(overlappedIdListUrl));
    m_httpClient->doGet(overlappedIdListUrl);
}

void HanwhaChunkLoader::sendTimelineRequest()
{
    if (m_terminated)
        return;

    NX_ASSERT(!m_overlappedIds.empty());
    if (m_overlappedIds.empty())
    {
        m_state = State::loadingOverlappedIds;
        setError();
        scheduleNextRequest(kResendRequestIfFail);
        return;
    }

    NX_ASSERT(m_currentOverlappedId != m_overlappedIds.cend());
    if (m_currentOverlappedId == m_overlappedIds.cend())
    {
        m_state = State::loadingOverlappedIds;
        setError();
        scheduleNextRequest(kResendRequestIfFail);
        return;
    }

    m_lastParsedStartTimeMs = AV_NOPTS_VALUE;
    if (!hasBounds())
    {
        m_startTimeUs = AV_NOPTS_VALUE;
        m_endTimeUs = AV_NOPTS_VALUE;
    }

    bool arePreviousChunksInvalid = m_overlappedIds.back() != m_lastNvrOverlappedId;
    if (arePreviousChunksInvalid)
    {
        m_chunks.clear();
        m_startTimeUs = AV_NOPTS_VALUE;
        m_endTimeUs = AV_NOPTS_VALUE;
    }

    m_lastNvrOverlappedId = m_overlappedIds.back();

    const auto overlappedId = m_isNvr
        ? QString::number(m_overlappedIds.back())
        : QString::number(*m_currentOverlappedId);

    using P = HanwhaRequestHelper::Parameters::value_type;
    HanwhaRequestHelper::Parameters parameters = {
        P(kHanwhaRecordingTypeProperty, kHanwhaAll),
        P(kHanwhaFromDateProperty, makeStartDateTimeString()),
        P(kHanwhaToDateProperty, makeEndDateTimeSting()),
        P(kHanwhaOverlappedIdProperty, overlappedId)
    };

    if (m_isNvr)
        parameters.emplace(kHanwhaChannelIdListProperty, makeChannelIdListString());

    const auto timelineUrl = HanwhaRequestHelper::buildRequestUrl(
        m_resourceContext,
        lit("recording/timeline/view"),
        parameters);

    NX_DEBUG(this, lm("Sending timeline request, URL: %1").arg(timelineUrl));
    prepareHttpClient();
    m_httpClient->setOnSomeMessageBodyAvailable([this]() { at_gotChunkData(); });
    m_httpClient->doGet(timelineUrl); //< TODO: Use m_resourceConext->requestSemaphore().
}

void HanwhaChunkLoader::handleSuccessfulRecordingPeriodResponse()
{
    if (m_terminated)
        return;

    NX_ASSERT(m_state == State::loadingRecordingPeriod);
    if (m_state != State::loadingRecordingPeriod)
    {
        m_state = nextState(State::initial);
        setError();
        return scheduleNextRequest(kResendRequestIfFail);
    }

    NX_DEBUG(this, lit("Handling successful recording period response."));

    parseTimeRangeData(m_httpClient->fetchMessageBodyBuffer());
    m_state = nextState(m_state);
    m_wait.wakeAll();
    sendRequest(); //< Send next request immediately
}

void HanwhaChunkLoader::handleSuccessfulOverlappedIdResponse()
{
    if (m_terminated)
        return;

    NX_ASSERT(m_state == State::loadingOverlappedIds);
    if (m_state != State::loadingOverlappedIds)
    {
        m_state = nextState(State::initial);
        setError();
        return scheduleNextRequest(kResendRequestIfFail);
    }

    NX_DEBUG(this, lit("Handling successful overlapped ID response"));

    m_overlappedIds.clear();
    parseOverlappedIdListData(m_httpClient->fetchMessageBodyBuffer());
    if (m_overlappedIds.empty())
    {
        NX_DEBUG(this, lit("Overlapped ID list is empty. Trying one more time."));
        setError();
        scheduleNextRequest(kResendRequestIfFail);
        return;
    }

    m_newChunks.clear();
    m_currentOverlappedId = m_overlappedIds.cbegin();
    m_state = nextState(m_state);
    sendRequest();
}

void HanwhaChunkLoader::handleSuccessfulTimelineResponse()
{
    if (m_terminated)
        return;

    NX_ASSERT(m_state == State::loadingTimeline);
    if (m_state != State::loadingTimeline)
    {
        m_state = nextState(State::initial);
        setError();
        scheduleNextRequest(kResendRequestIfFail);
    }

    NX_DEBUG(this, lit("Handling successful timeline response."));
    if (isEdge())
    {
        // In case of edge archive import we should load chunks for all existing overlapped IDs.
        ++m_currentOverlappedId;
        if (m_currentOverlappedId != m_overlappedIds.cend())
        {
            NX_DEBUG(
                this,
                lm("Camera has more overlapped data. Asking for overlapped timline %1")
                    .arg(*m_currentOverlappedId));

            sendTimelineRequest();
            return;
        }

        //< Cameras sometimes send unordered list of chunks.
        sortTimeline(&m_newChunks);
        m_chunks.swap(m_newChunks);
    }

    m_lastTimelineUpdate = std::chrono::milliseconds(
        qnSyncTime->currentMSecsSinceEpoch());

    m_state = nextState(m_state);
    m_wait.wakeAll();
    scheduleNextRequest(kUpdateChunksDelay); //< Send next request after delay
}

bool HanwhaChunkLoader::handleHttpError()
{
    auto scopeGuard = nx::utils::makeScopeGuard(
        [this]()
        {
            setError();
            scheduleNextRequest(kResendRequestIfFail);
        });

    if (m_httpClient->state() == nx::network::http::AsyncClient::State::sFailed)
    {
        NX_WARNING(
            this,
            lm("HTTP request %1 failed with error %2")
                .args(
                    m_httpClient->contentLocationUrl(),
                    m_httpClient->lastSysErrorCode()));

        return true;
    }

    const auto statusCode = m_httpClient->response()->statusLine.statusCode;
    if (statusCode != nx::network::http::StatusCode::ok)
    {
        NX_WARNING(
            this,
            lm("HTTP request %1 failed with status code %2")
                .args(
                    m_httpClient->contentLocationUrl(),
                    statusCode));

        return true;
    }

    scopeGuard.disarm();
    return false;
}

boost::optional<HanwhaChunkLoader::Parameter> HanwhaChunkLoader::parseLine(
    const nx::Buffer& line) const
{
    const auto separatorPosition = line.indexOf('=');
    if (separatorPosition < 0)
        return boost::none;

    return Parameter(
        line.left(separatorPosition).trimmed(),
        line.mid(separatorPosition + 1).trimmed());
}

void HanwhaChunkLoader::parseTimeRangeData(const nx::Buffer& data)
{
    NX_ASSERT(hasBounds());
    auto startTimeUs = AV_NOPTS_VALUE;
    auto endTimeUs = AV_NOPTS_VALUE;
    for (const auto& line: data.split('\n'))
    {
        const auto params = line.trimmed().split('=');
        if (params.size() == 2)
        {
            const auto& fieldName = params[0];
            const auto& fieldValue = params[1];

            if (fieldName == kStartTimeParamName)
                startTimeUs = hanwhaDateTimeToMsec(fieldValue, m_timeShift) * 1000;
            else if (fieldName == kEndTimeParamName)
                endTimeUs = hanwhaDateTimeToMsec(fieldValue, m_timeShift) * 1000;
        }
    }

    if (startTimeUs != AV_NOPTS_VALUE && endTimeUs != AV_NOPTS_VALUE)
    {
        m_startTimeUs = startTimeUs;
        m_endTimeUs = endTimeUs;
        const auto startTimeMs = m_startTimeUs / 1000;

        for (auto& entry: m_chunks)
        {
            const auto overlappedId = entry.first;
            auto& chunksByChannel = entry.second;

            for (auto& chunks: chunksByChannel)
            {
                while (!chunks.isEmpty() && chunks.first().endTimeMs() < startTimeMs)
                    chunks.pop_front();
            }
        }
    }
    else
    {
        NX_WARNING(this, lm("Can't parse time range response %1").arg(data));
    }
}

bool HanwhaChunkLoader::parseTimelineData(const nx::Buffer& line, qint64 currentTimeMs)
{
    const auto channelNumberPos = line.indexOf('.') + 1;
    if (channelNumberPos == 0)
        return true; //< Skip header line

    const auto channelNumberPosEnd = line.indexOf('.', channelNumberPos + 1);
    const auto channelNumber = line.mid(
        channelNumberPos,
        channelNumberPosEnd - channelNumberPos)
            .toInt();

    if (channelNumber > kMaxAllowedChannelNumber)
    {
        NX_WARNING(this, lm("Ignore line %1 due to too big channel number %2 provided")
            .args(line, channelNumber));
        return false;
    }

    NX_ASSERT(m_currentOverlappedId != m_overlappedIds.cend());
    if (m_currentOverlappedId == m_overlappedIds.cend())
        return false;

    auto& chunksByChannel = hasBounds()
        ? m_chunks[*m_currentOverlappedId]
        : m_newChunks[*m_currentOverlappedId];

    if (chunksByChannel.size() <= channelNumber)
        chunksByChannel.resize(channelNumber + 1);

    const auto fieldNamePos = line.lastIndexOf('.') + 1;
    const auto delimiterPos = line.lastIndexOf('=');
    const auto fieldName = line.mid(fieldNamePos, delimiterPos - fieldNamePos);
    const auto fieldValue = line.mid(delimiterPos + 1).trimmed();

    QnTimePeriodList& chunks = chunksByChannel[channelNumber];
    if (fieldName == kStartTimeParamName)
    {
        m_lastParsedStartTimeMs = hanwhaDateTimeToMsec(fieldValue, m_timeShift);
    }
    else if (fieldName == kEndTimeParamName)
    {
        // Hanwha API reports closed periods: [a, b - 1s], [b, c - 1s], ...,
        // while our VMS and the rest of the world uses open ended periods: [a, b), [b, c), ....
        static const qint64 kEndTimeFixMs = 1000;

		m_lastParsedEndTimeMs = hanwhaDateTimeToMsec(fieldValue, m_timeShift) + kEndTimeFixMs;
    }
    else if (fieldName == kTypeParamName)
    {
        if (m_lastParsedStartTimeMs > currentTimeMs)
        {
            NX_DEBUG(this, lm("Ignore period [%1, %2] from future on channel %3")
                .args(m_lastParsedStartTimeMs, m_lastParsedEndTimeMs, channelNumber));
            return false;
        }

        QnTimePeriod timePeriod(m_lastParsedStartTimeMs, m_lastParsedEndTimeMs - m_lastParsedStartTimeMs);
        if (m_startTimeUs == AV_NOPTS_VALUE || chunks.isEmpty())
            m_startTimeUs = timePeriod.startTimeMs * 1000;

        if (!m_isNvr)
        {
            // We shouldn't merge periods for cameras because of bug
            // with streaming on borders of chunks.
            chunks.push_back(timePeriod);
        }
        else
        {
            QnTimePeriodList periods;
            periods << timePeriod;
            if (fieldValue == "Normal")
                QnTimePeriodList::overwriteTail(chunks, periods, timePeriod.startTimeMs);
            else
                QnTimePeriodList::unionTimePeriods(chunks, periods);

        }
    }

    return true;
}

void HanwhaChunkLoader::parseOverlappedIdListData(const nx::Buffer& data)
{
    for (const auto& line: data.split('\n'))
    {
        const auto parameter = parseLine(line.trimmed());
        if (parameter == boost::none)
            continue;

        if (parameter->name != kOverlappedIdListParameter)
            continue;

        const auto split = parameter->value.split(L',');
        if (split.isEmpty())
            continue;

        for (const auto& idString: split)
        {
            bool success = false;
            auto id = idString.trimmed().toInt(&success);
            if (success)
                m_overlappedIds.push_back(id);
        }
    }

    std::sort(m_overlappedIds.begin(), m_overlappedIds.end());
}

void HanwhaChunkLoader::prepareHttpClient()
{
    const auto authenticator = m_resourceContext->authenticator();
    m_httpClient = std::make_unique<nx::network::http::AsyncClient>();
    m_httpClient->setUserName(authenticator.user());
    m_httpClient->setUserPassword(authenticator.password());
    m_httpClient->setSendTimeout(kSendTimeout);
    m_httpClient->setResponseReadTimeout(m_settings.responseTimeout);
    m_httpClient->setMessageBodyReadTimeout(m_settings.messageBodyReadTimeout);
    m_httpClient->setOnDone([this](){ at_httpClientDone(); });
}

void HanwhaChunkLoader::scheduleNextRequest(const std::chrono::milliseconds& delay)
{
    if (m_terminated)
        return;

    if (!m_started)
        return;

    m_nextRequestTimerId = nx::utils::TimerManager::instance()->addTimer(
        [this](nx::utils::TimerId timerId)
        {
            if (timerId != m_nextRequestTimerId)
                return;

            QnMutexLocker lock(&m_mutex);
            sendRequest();
        },
        delay);
}

qint64 HanwhaChunkLoader::latestChunkTimeMs() const
{
    qint64 resultMs = 0;
    for (const auto& entry: m_chunks)
    {
        const auto overlappedId = entry.first;
        const auto& chunksByChannel = entry.second;

        for (const auto& chunks: chunksByChannel)
        {
            if (!chunks.empty())
                resultMs = std::max(resultMs, chunks.back().startTimeMs);
        }
    }

    if (m_startTimeUs == AV_NOPTS_VALUE)
        return resultMs;

    return std::max(resultMs, m_startTimeUs / 1000);
}

void HanwhaChunkLoader::sortTimeline(OverlappedChunks* outTimeline) const
{
    for (auto& entry: *outTimeline)
    {
        for (auto& channelEntry: entry.second)
        {
            std::sort(
                channelEntry.begin(),
                channelEntry.end(),
                [](const QnTimePeriod& lhs, const QnTimePeriod& rhs)
                {
                    return lhs.startTimeMs < rhs.startTimeMs;
                });
        }
    }
}

void HanwhaChunkLoader::setError()
{
    if (!m_started)
        m_state = State::initial;

    m_wait.wakeAll();
}

HanwhaChunkLoader::State HanwhaChunkLoader::nextState(State currentState) const
{
    switch (currentState)
    {
        case State::initial:
        {
            return hasBounds()
                ? State::loadingRecordingPeriod
                : State::loadingOverlappedIds;
        }
        case State::loadingTimeline:
        {
            if (!m_started)
                return State::initial;

            return hasBounds()
                ? State::loadingRecordingPeriod
                : State::loadingOverlappedIds;
        }
        case State::loadingRecordingPeriod:
        {
            return State::loadingOverlappedIds;
        }
        case State::loadingOverlappedIds:
        {
            return State::loadingTimeline;
        }
        default:
        {
            NX_ASSERT(false, lit("Wrong state, should not be here"));
            return State::loadingOverlappedIds;
        }
    }
}

QString HanwhaChunkLoader::makeChannelIdListString() const
{
    QStringList channels;
    for (int i = 0; i < m_maxChannels; ++i)
        channels << QString::number(i);

    return channels.join(L',');
}

QString HanwhaChunkLoader::makeStartDateTimeString() const
{
    auto updateLagMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        kUpdateChunksDelay).count();

    const auto startDateTime = hasBounds()
        ? toHanwhaDateTime(latestChunkTimeMs() - updateLagMs)
        : kMinDateTime;

    return convertDateToString(startDateTime);
}

QString HanwhaChunkLoader::makeEndDateTimeSting() const
{
    auto endDateTime = hasBounds()
        ? QDateTime::fromMSecsSinceEpoch(std::numeric_limits<int>::max() * 1000ll).addDays(-1)
        : kMaxDateTime;

    return convertDateToString(endDateTime);
}

std::chrono::milliseconds HanwhaChunkLoader::timeSinceLastTimelineUpdate() const
{
    return std::chrono::milliseconds(qnSyncTime->currentMSecsSinceEpoch())
        - m_lastTimelineUpdate;
}

void HanwhaChunkLoader::setUpThreadUnsafe(const HanwhaInformation& information)
{
    if (m_state != State::initial)
        return; //< Already started

    m_hasSearchRecordingPeriodSubmenu = false;
    const auto searchRecordingPeriodAttribute = information.attributes.attribute<bool>(
        lit("Recording/SearchPeriod"));

    if (searchRecordingPeriodAttribute != boost::none)
        m_hasSearchRecordingPeriodSubmenu = searchRecordingPeriodAttribute.get();

    m_isNvr = information.deviceType == HanwhaDeviceType::nvr;
    m_maxChannels = information.channelCount;
}

OverlappedTimePeriods HanwhaChunkLoader::overlappedTimelineThreadUnsafe(
    int channelNumber) const
{
    const bool needToRestrictPeriods = hasBounds()
        && m_startTimeUs != AV_NOPTS_VALUE
        && m_endTimeUs != AV_NOPTS_VALUE;

    const auto startTimeMs = m_startTimeUs / 1000;
    const auto endTimeMs = m_endTimeUs / 1000;
    QnTimePeriod boundingPeriod(startTimeMs, endTimeMs - startTimeMs);

    OverlappedTimePeriods result;
    for (auto itr = m_chunks.rbegin(); itr != m_chunks.rend(); ++itr)
    {
        const auto overlappedId = itr->first;
        const auto& chunksByChannel = itr->second;

        if (channelNumber >= chunksByChannel.size())
        {
            if (isNvr())
                break;
            else
                continue;
        }

        if (needToRestrictPeriods)
            result[overlappedId] = chunksByChannel[channelNumber].intersected(boundingPeriod);
        else
            result[overlappedId] = chunksByChannel[channelNumber];

        // In the case of NVR we must provide only chunks that
        // belong to the latest overlapped ID.
        if (isNvr())
            break;
    }

    return result;
}

void HanwhaChunkLoader::at_httpClientDone()
{
    if (m_terminated)
        return;

    QnMutexLocker lock(&m_mutex);
    if (handleHttpError())
        return; //< Some error has occurred

    NX_VERBOSE(this, lm("Http request %1 succeeded with status code %2")
        .args(m_httpClient->contentLocationUrl(), m_httpClient->lastSysErrorCode()));

    switch (m_state)
    {
        case State::loadingRecordingPeriod:
            handleSuccessfulRecordingPeriodResponse();
            break;
        case State::loadingOverlappedIds:
            handleSuccessfulOverlappedIdResponse();
            break;
        case State::loadingTimeline:
            handleSuccessfulTimelineResponse();
            break;
        default:
            NX_ASSERT(false, lit("Wrong state, shouldn't be here"));
    }
}

void HanwhaChunkLoader::at_gotChunkData()
{
    if (m_terminated)
        return;

    QnMutexLocker lock(&m_mutex);
    // This function should be fast because of large amount of records for recorded data

    auto buffer = m_unfinishedLine;
    buffer.append(m_httpClient->fetchMessageBodyBuffer());
    int index = buffer.lastIndexOf('\n');
    if (index == -1)
    {
        m_unfinishedLine = buffer;
        return;
    }

    m_unfinishedLine = buffer.mid(index + 1);
    buffer.truncate(index);

    const auto lines = buffer.split('\n');
    NX_VERBOSE(this, lm("%1 got %2 lines of chunk data")
        .args(m_httpClient->contentLocationUrl(), lines.size()));

    const auto currentTimeMs = qnSyncTime->currentMSecsSinceEpoch();
    size_t parsedChunks = 0;
    for (const auto& line: lines)
    {
        if (parseTimelineData(line.trimmed(), currentTimeMs))
            parsedChunks++;
    }

    NX_VERBOSE(this, "Overlapped Id %1, parsed %2 chunks", *m_currentOverlappedId, parsedChunks);
}

bool HanwhaChunkLoader::isEdge() const
{
    return !m_isNvr;
}

bool HanwhaChunkLoader::isNvr() const
{
    return m_isNvr;
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
