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

static const std::chrono::milliseconds kHttpTimeout(8000);

HanwhaChunkLoader::HanwhaChunkLoader():
    m_httpClient(std::make_unique<nx_http::AsyncClient>())
{
}

HanwhaChunkLoader::~HanwhaChunkLoader()
{
    m_terminated = true;
    if (m_nextRequestTimerId)
        nx::utils::TimerManager::instance()->deleteTimer(m_nextRequestTimerId);
    m_httpClient->pleaseStopSync();
}

void HanwhaChunkLoader::start(HanwhaSharedResourceContext* resourceContext)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (m_state != State::initial)
            return; //< Already started

        const auto information = resourceContext->information();
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
        m_resourceContext = resourceContext;
        m_state = nextState(m_state);
        NX_DEBUG(this, lm("Started for %1 channels on %2")
            .args(m_maxChannels, resourceContext->url()));
    }

    sendRequest();
}

bool HanwhaChunkLoader::isStarted() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state != State::initial;
}

qint64 HanwhaChunkLoader::startTimeUsec(int channelNumber) const
{
    QnMutexLocker lock(&m_mutex);
    if (m_chunks.size() <= channelNumber || m_chunks[channelNumber].isEmpty())
        return AV_NOPTS_VALUE;

    auto result = m_chunks[channelNumber].front().startTimeMs * 1000;
    if (m_startTimeUs != AV_NOPTS_VALUE)
        result = std::max(m_startTimeUs, result);

    return result;
}

qint64 HanwhaChunkLoader::endTimeUsec(int channelNumber) const
{
    QnMutexLocker lock(&m_mutex);
    if (m_chunks.size() <= channelNumber || m_chunks[channelNumber].isEmpty())
        return AV_NOPTS_VALUE;

    return m_chunks[channelNumber].last().endTimeMs() * 1000;
}

QnTimePeriodList HanwhaChunkLoader::chunks(int channelNumber) const
{
    QnMutexLocker lock(&m_mutex);
    return chunksThreadUnsafe(channelNumber);
}

QnTimePeriodList HanwhaChunkLoader::chunksSync(int channelNumber) const
{
    QnMutexLocker lock(&m_mutex);

    while (!m_timelineLoadedAtLeastOnce && !m_errorOccured)
        m_wait.wait(&m_mutex);

    return chunksThreadUnsafe(channelNumber);
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
    return m_overlappedId;
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
        && m_isSearchRecordingPeriodRetrievalEnabled;
}

//------------------------------------------------------------------------------------------------

void HanwhaChunkLoader::sendRequest()
{
    switch (m_state)
    {
        case State::updateTimeRange:
            sendTimeRangeRequest();
            break;
        case State::loadingOverlappedIds:
            sendOverlappedIdRequest();
            break;
        case State::loadingChunks:
            sendTimelineRequest();
            break;
        default:
            NX_ASSERT(false, lit("Wrong state, should not be here."));
            break;
    }
}

void HanwhaChunkLoader::sendTimeRangeRequest()
{
    prepareHttpClient();

    // TODO: Check for 'attributes/Recording/Support/SearchPeriod' and use only if it's supported,
    // otherwise we have to load all periods constantly and clean them up by timeout.
    const auto loadChunksUrl = HanwhaRequestHelper::buildRequestUrl(
        m_resourceContext,
        lit("recording/searchrecordingperiod/view"),
        {
            {kHanwhaRecordingTypeProperty, kHanwhaAll},
            {kHanwhaResultsInUtcProperty, m_isUtcEnabled ? kHanwhaTrue : kHanwhaFalse}
        });

    m_httpClient->setOnSomeMessageBodyAvailable(nullptr);
    m_httpClient->doGet(loadChunksUrl); //< TODO: Use m_resourceConext->requestSemaphore().
}

void HanwhaChunkLoader::handleSuccessfulTimeRangeResponse()
{
    NX_ASSERT(m_state == State::updateTimeRange);
    if (m_state != State::updateTimeRange)
        return scheduleNextRequest(kResendRequestIfFail);

    parseTimeRangeData(m_httpClient->fetchMessageBodyBuffer());
    m_timeRangeLoadedAtLeastOnce = true;
    m_errorOccured = false;
    m_state = nextState(m_state);
    m_wait.wakeAll();
    sendRequest(); //< Send next request immediately
}

void HanwhaChunkLoader::handleSuccessfulOverlappedIdResponse()
{
    NX_ASSERT(m_state == State::loadingOverlappedIds);
    if (m_state != State::loadingOverlappedIds)
        return scheduleNextRequest(kResendRequestIfFail);

    parseOverlappedIdListData(m_httpClient->fetchMessageBodyBuffer());
    if (m_overlappedId == boost::none)
    {
        setError();
        scheduleNextRequest(kResendRequestIfFail);
        return;
    }

    m_state = nextState(m_state);
    sendRequest();
}

void HanwhaChunkLoader::handlerSuccessfulChunksResponse()
{
    NX_ASSERT(m_state == State::loadingChunks);
    if (m_state != State::loadingChunks)
        scheduleNextRequest(kResendRequestIfFail);

    {
        QnMutexLocker lock(&m_mutex);
        if (!hasBounds())
            m_chunks.swap(m_newChunks);
        m_errorOccured = false;
        m_timelineLoadedAtLeastOnce = true;
    }

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
            m_wait.wakeAll();
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

void HanwhaChunkLoader::sendOverlappedIdRequest()
{
    prepareHttpClient();
    const auto overlappedIdListUrl = HanwhaRequestHelper::buildRequestUrl(
        m_resourceContext,
        lit("recording/overlapped/view"),
        {
            {kHanwhaChannelIdListProperty, makeChannelIdListString()},
            {kHanwhaFromDateProperty, makeStartDateTimeString()},
            {kHanwhaToDateProperty, makeEndDateTimeSting()}
        });

    m_httpClient->doGet(overlappedIdListUrl);
}

void HanwhaChunkLoader::sendTimelineRequest()
{
    NX_ASSERT(m_overlappedId != boost::none);
    if (m_overlappedId == boost::none)
    {
        m_state = State::loadingOverlappedIds;
        scheduleNextRequest(kResendRequestIfFail);
    }

    {
        QnMutexLocker lock(&m_mutex);
        m_newChunks.clear();
        m_lastParsedStartTimeMs = AV_NOPTS_VALUE;
        if (!hasBounds())
        {
            m_startTimeUs = AV_NOPTS_VALUE;
            m_endTimeUs = AV_NOPTS_VALUE;
        }
    }

    using P = HanwhaRequestHelper::Parameters::value_type;
    HanwhaRequestHelper::Parameters parameters = {
        P(kHanwhaRecordingTypeProperty, kHanwhaAll),
        P(kHanwhaFromDateProperty, makeStartDateTimeString()),
        P(kHanwhaToDateProperty, makeEndDateTimeSting()),
        P(kHanwhaOverlappedIdProperty, QString::number(*m_overlappedId))
    };

    if (m_isNvr)
        parameters.emplace(kHanwhaChannelIdListProperty, makeChannelIdListString());

    const auto loadChunksUrl = HanwhaRequestHelper::buildRequestUrl(
        m_resourceContext,
        lit("recording/timeline/view"),
        parameters);

    prepareHttpClient();
    m_httpClient->setOnSomeMessageBodyAvailable([this](){ at_gotChunkData(); });
    m_httpClient->doGet(loadChunksUrl); //< TODO: Use m_resourceConext->requestSemaphore().
}

void HanwhaChunkLoader::parseTimeRangeData(const nx::Buffer& data)
{
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

        QnMutexLocker lock(&m_mutex);
        for (auto& chunks: m_chunks)
        {
            while (!chunks.isEmpty() && chunks.first().endTimeMs() < startTimeMs)
                chunks.pop_front();
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

    auto& chunksByChannel = hasBounds()
        ? m_chunks
        : m_newChunks;

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
        {
            chunks.push_back(timePeriod);
            m_startTimeUs = timePeriod.startTimeMs * 1000;
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

        bool success = false;
        const auto overlappedId = (m_isNvr
            ? split.first()
            : split.last())
                .toInt(&success);

        if (success)
            m_overlappedId = overlappedId;
    }
}

void HanwhaChunkLoader::prepareHttpClient()
{
    const auto authenticator = m_resourceContext->authenticator();
    QnMutexLocker lock(&m_mutex);
    m_httpClient->setUserName(authenticator.user());
    m_httpClient->setUserPassword(authenticator.password());
    m_httpClient->setResponseReadTimeout(kHttpTimeout);
    m_httpClient->setOnDone([this](){ at_httpClientDone(); });
}

void HanwhaChunkLoader::scheduleNextRequest(const std::chrono::milliseconds& delay)
{
    if (m_terminated)
        return;

    m_nextRequestTimerId = nx::utils::TimerManager::instance()->addTimer(
        [this](nx::utils::TimerId /*timerId*/) { sendRequest(); },
        delay);
}

qint64 HanwhaChunkLoader::latestChunkTimeMs() const
{
    qint64 resultMs = 0;
    for (const auto& timePeriods : m_chunks)
    {
        if (!timePeriods.empty())
            resultMs = std::max(resultMs, timePeriods.back().startTimeMs);
    }

    QnMutexLocker lock(&m_mutex);
    if (m_startTimeUs == AV_NOPTS_VALUE)
        return resultMs;

    return std::max(resultMs, m_startTimeUs / 1000);
}

QnTimePeriodList HanwhaChunkLoader::chunksThreadUnsafe(int channelNumber) const
{
    const qint64 startTimeMs = m_startTimeUs / 1000;
    const qint64 endTimeMs = m_endTimeUs / 1000;
    if (m_chunks.size() <= channelNumber)
        return QnTimePeriodList();

    QnTimePeriod boundingPeriod(startTimeMs, endTimeMs - startTimeMs);

    if (hasBounds())
        return m_chunks[channelNumber].intersected(boundingPeriod);

    return m_chunks[channelNumber];
}

void HanwhaChunkLoader::setError()
{
    m_errorOccured = true;
    m_timelineLoadedAtLeastOnce = false;
    m_timeRangeLoadedAtLeastOnce = false;
}

HanwhaChunkLoader::State HanwhaChunkLoader::nextState(State currentState) const
{
    switch (currentState)
    {
        case State::initial:
        case State::loadingChunks:
        {
            return hasBounds()
                ? State::updateTimeRange
                : State::loadingOverlappedIds;
        }
        case State::updateTimeRange:
        {
            return State::loadingOverlappedIds;
        }
        case State::loadingOverlappedIds:
        {
            return State::loadingChunks;
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

void HanwhaChunkLoader::at_httpClientDone()
{
    if (handleHttpError())
        return; //< Some error has occurred

    NX_VERBOSE(this, lm("Http request %1 succeeded with status code %2")
        .args(m_httpClient->contentLocationUrl(), m_httpClient->lastSysErrorCode()));

    switch (m_state)
    {
        case State::updateTimeRange:
            handleSuccessfulTimeRangeResponse();
            break;
        case State::loadingOverlappedIds:
            handleSuccessfulOverlappedIdResponse();
            break;
        case State::loadingChunks:
            handlerSuccessfulChunksResponse();
            break;
        default:
            NX_ASSERT(false, lit("Wrong state, shouldn't be here"));
    }
}

void HanwhaChunkLoader::at_gotChunkData()
{
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

    {
        QnMutexLocker lock(&m_mutex);
        const auto currentTimeMs = qnSyncTime->currentMSecsSinceEpoch();
        for (const auto& line : lines)
            parseTimelineData(line.trimmed(), currentTimeMs);
    }
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
