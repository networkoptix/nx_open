#if defined(ENABLE_HANWHA)

#include <QtCore/QDateTime>
#include <QtCore/QUrlQuery>

#include <nx/utils/log/log_main.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/scope_guard.h>
#include <utils/common/synctime.h>
#include <core/resource/network_resource.h>

#include "hanwha_chunk_reader.h"
#include "hanwha_request_helper.h"
#include "hanwha_shared_resource_context.h"
#include "hanwha_utils.h"

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

static const int kMaxAllowedChannelNumber = 64;
static const nx::Buffer kStartTimeParamName("StartTime");
static const nx::Buffer kEndTimeParamName("EndTime");
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

static const std::chrono::milliseconds kHttpTimeout(10000);
static const std::chrono::milliseconds kTimelineCacheTime(10000);

} // namespace

using namespace nx::core::resource;

HanwhaChunkLoader::HanwhaChunkLoader(HanwhaSharedResourceContext* resourceContext):
    m_resourceContext(resourceContext)
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

void HanwhaChunkLoader::setUp()
{
    QnMutexLocker lock(&m_mutex);
    setUpThreadUnsafe();
}

void HanwhaChunkLoader::start(bool isNvr)
{
    QnMutexLocker lock(&m_mutex);
    setUpThreadUnsafe();

    if (isNvr)
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

void HanwhaChunkLoader::setTimeZoneShift(std::chrono::seconds timeZoneShift)
{
    m_timeZoneShift = timeZoneShift;
}

std::chrono::seconds HanwhaChunkLoader::timeZoneShift() const
{
    return m_timeZoneShift;
}

boost::optional<int> HanwhaChunkLoader::overlappedId() const
{
    QnMutexLocker lock(&m_mutex);
    NX_ASSERT(m_isNvr, lit("Method should be called only for NVRs"));
    if (m_isNvr)
        return m_overlappedIds.front();

    // For cameras we should import all chunks from all overlapped IDs.
    return boost::none;
}

void HanwhaChunkLoader::setEnableUtcTime(bool enableUtcTime)
{
    m_isUtcEnabled = enableUtcTime;
}

void HanwhaChunkLoader::setEnableSearchRecordingPeriodRetieval(bool enableRetrieval)
{
    m_isSearchRecordingPeriodRetrievalEnabled = enableRetrieval;
}

QString HanwhaChunkLoader::convertDateToString(const QDateTime& dateTime) const
{
    return dateTime.toString(m_isUtcEnabled ? kHanwhaUtcDateTimeFormat : kHanwhaDateTimeFormat);
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
            {kHanwhaResultsInUtcProperty, m_isUtcEnabled ? kHanwhaTrue : kHanwhaFalse}
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
            {kHanwhaFromDateProperty, makeStartDateTimeString()},
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

    const auto overlappedId = m_isNvr
        ? QString::number(m_overlappedIds.front())
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
    // In case of edge archive import we should load chunks for all existing overlapped IDs.
    if (!m_isNvr)
    {
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
    }

    if (isEdge()) //< Cameras sometimes send unordered list of chunks.
    {
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
    auto scopeGuard = makeScopeGuard(
        [this]()
        {
            setError();
            scheduleNextRequest(kResendRequestIfFail);
        });

    if (m_httpClient->state() == nx_http::AsyncClient::sFailed)
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
    if (statusCode != nx_http::StatusCode::ok)
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
                startTimeUs = hanwhaDateTimeToMsec(fieldValue, m_timeZoneShift) * 1000;
            else if (fieldName == kEndTimeParamName)
                endTimeUs = hanwhaDateTimeToMsec(fieldValue, m_timeZoneShift) * 1000;
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
        m_lastParsedStartTimeMs = hanwhaDateTimeToMsec(fieldValue, m_timeZoneShift);
    }
    else if (fieldName == kEndTimeParamName)
    {
        const auto endTimeMs = hanwhaDateTimeToMsec(fieldValue, m_timeZoneShift);
        if (m_lastParsedStartTimeMs > currentTimeMs)
        {
            NX_DEBUG(this, lm("Ignore period [%1, %2] from future on channel %3")
                .args(m_lastParsedStartTimeMs, endTimeMs, channelNumber));
            return false;
        }

        QnTimePeriod timePeriod(m_lastParsedStartTimeMs, endTimeMs - m_lastParsedStartTimeMs);
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
            QnTimePeriodList::overwriteTail(chunks, periods, timePeriod.startTimeMs);
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
}

void HanwhaChunkLoader::prepareHttpClient()
{
    const auto authenticator = m_resourceContext->authenticator();
    m_httpClient = std::make_unique<nx_http::AsyncClient>();
    m_httpClient->setUserName(authenticator.user());
    m_httpClient->setUserPassword(authenticator.password());
    m_httpClient->setSendTimeout(kHttpTimeout);
    m_httpClient->setResponseReadTimeout(kHttpTimeout);
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

    const auto timeZoneShift = m_isUtcEnabled
        ? std::chrono::seconds::zero()
        : std::chrono::seconds(m_timeZoneShift);

    auto startDateTime = hasBounds()
        ? toHanwhaDateTime(latestChunkTimeMs() - updateLagMs, timeZoneShift)
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

void HanwhaChunkLoader::setUpThreadUnsafe()
{
    if (m_state != State::initial)
        return; //< Already started

    const auto information = m_resourceContext->information();
    if (!information)
    {
        NX_DEBUG(this, lit("Unable to fetch channel number"));
        return; //< Unable to start without channel number.
    }

    m_hasSearchRecordingPeriodSubmenu = false;
    const auto searchRecordingPeriodAttribute = information->attributes.attribute<bool>(
        lit("Recording/SearchPeriod"));

    if (searchRecordingPeriodAttribute != boost::none)
        m_hasSearchRecordingPeriodSubmenu = searchRecordingPeriodAttribute.get();

    m_isNvr = information->deviceType == kHanwhaNvrDeviceType;
    m_maxChannels = information->channelCount;
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
    for (const auto& entry : m_chunks)
    {
        const auto overlappedId = entry.first;
        const auto& chunksByChannel = entry.second;

        if (chunksByChannel.size() < channelNumber + 1)
            continue;

        if (needToRestrictPeriods)
            result[overlappedId] = chunksByChannel[channelNumber].intersected(boundingPeriod);
        else
            result[overlappedId] = chunksByChannel[channelNumber];
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
    for (const auto& line: lines)
        parseTimelineData(line.trimmed(), currentTimeMs);
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
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
