#if defined(ENABLE_HANWHA)

#include <QtCore/QDateTime>
#include <QtCore/QUrlQuery>

#include <nx/utils/log/log_main.h>
#include <nx/utils/timer_manager.h>
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
static const char kStartTimeParamName[] = "StartTime";
static const char kEndTimeParamName[] = "EndTime";

static std::chrono::seconds kUpdateChunksDelay(60);
static std::chrono::seconds kResendRequestIfFail(10);

HanwhaChunkLoader::HanwhaChunkLoader():
    m_httpClient(new nx_http::AsyncClient())
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
        if (m_state != State::Initial)
            return; //< Already started

        if (auto information = resourceContext->information())
            m_maxChannels = information->channelCount;
        else
            return; //< Unable to start without channel number.

        m_resourceContext = resourceContext;
        m_state = nextState(m_state);
        NX_DEBUG(this, lm("Started for %1 channels on %2").args(m_maxChannels, resourceContext->url()));
    }

    sendRequest();
}

bool HanwhaChunkLoader::isStarted() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state != State::Initial;
}

void HanwhaChunkLoader::sendRequest()
{
    switch (m_state)
    {
        case State::updateTimeRange:
            sendUpdateTimeRangeRequest();
            break;
        case State::LoadingChunks:
            sendLoadChunksRequest();
            break;
        default:
            break;
    }
}

void HanwhaChunkLoader::sendUpdateTimeRangeRequest()
{
    {
        const auto authenticator = m_resourceContext->authenticator();
        QnMutexLocker lock(&m_mutex);
        m_httpClient->setUserName(authenticator.user());
        m_httpClient->setUserPassword(authenticator.password());
    }

    auto loadChunksUrl = m_resourceContext->url();
    loadChunksUrl.setPath("/stw-cgi/recording.cgi");

    QUrlQuery query;
    query.addQueryItem("msubmenu", "searchrecordingperiod");
    query.addQueryItem("action", "view");
    query.addQueryItem("Type", "All");
    loadChunksUrl.setQuery(query);

    m_httpClient->setOnDone(
        std::bind(&HanwhaChunkLoader::onHttpClientDone, this));
    m_httpClient->setOnSomeMessageBodyAvailable(nullptr);

    // TODO: Use m_resourceConext->requestSemaphore().
    m_httpClient->doGet(loadChunksUrl);
}

qint64 HanwhaChunkLoader::latestChunkTimeMs() const
{
    qint64 resultMs = 0;
    for (const auto& timePeriods: m_chunks)
    {
        if (!timePeriods.empty())
            resultMs = std::max(resultMs, timePeriods.back().startTimeMs);
    }

    QnMutexLocker lock(&m_mutex);
    if (m_startTimeUsec == AV_NOPTS_VALUE)
        return resultMs;
    return std::max(resultMs, m_startTimeUsec / 1000);
}

void HanwhaChunkLoader::sendLoadChunksRequest()
{
    {
        const auto authenticator = m_resourceContext->authenticator();
        QnMutexLocker lock(&m_mutex);
        m_httpClient->setUserName(authenticator.user());
        m_httpClient->setUserPassword(authenticator.password());
    }

    auto loadChunksUrl = m_resourceContext->url();
    loadChunksUrl.setPath("/stw-cgi/recording.cgi");

    QUrlQuery query;
    query.addQueryItem("msubmenu", "timeline");
    query.addQueryItem("action", "view");
    query.addQueryItem("Type", "All");

    auto updateLagMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        kUpdateChunksDelay).count();

    auto startDateTime = toHanwhaDateTime(latestChunkTimeMs() - updateLagMs, m_timeZoneShift);
    auto endDateTime = QDateTime::fromMSecsSinceEpoch(std::numeric_limits<int>::max() * 1000ll).addDays(-1);
    query.addQueryItem("FromDate", startDateTime.toString(kHanwhaDateFormat));
    query.addQueryItem("ToDate", endDateTime.toString(kHanwhaDateFormat));

    QStringList channelsParam;
    for (int i = 0; i < m_maxChannels; ++i)
        channelsParam << QString::number(i);

    if (m_resourceContext->information()->deviceType == kHanwhaNvrDeviceType)
        query.addQueryItem("ChannelIdList", channelsParam.join(','));

    loadChunksUrl.setQuery(query);
    m_lastParsedStartTimeMs = AV_NOPTS_VALUE;
    m_httpClient->setOnSomeMessageBodyAvailable(
        std::bind(&HanwhaChunkLoader::onGotChunkData, this));
    m_httpClient->setOnDone(
        std::bind(&HanwhaChunkLoader::onHttpClientDone, this));

    // TODO: Use m_resourceConext->requestSemaphore().
    m_httpClient->doGet(loadChunksUrl);
}

void HanwhaChunkLoader::onHttpClientDone()
{
    nx_http::AsyncClient::State state = m_httpClient->state();
    if (state == nx_http::AsyncClient::sFailed)
    {
        NX_WARNING(this, lm("Http request %1 failed with error %2")
            .args(m_httpClient->contentLocationUrl(), m_httpClient->lastSysErrorCode()));

        setError();
        m_wait.wakeAll();
        startTimerForNextRequest(kResendRequestIfFail);
        return;
    }
    const int statusCode = m_httpClient->response()->statusLine.statusCode;
    if (statusCode != nx_http::StatusCode::ok)
    {
        NX_WARNING(this, lm("Http request %1 failed with status code %2")
            .args(m_httpClient->contentLocationUrl(), m_httpClient->lastSysErrorCode()));

        setError();
        m_wait.wakeAll();
        startTimerForNextRequest(kResendRequestIfFail);
        return;
    }

    NX_VERBOSE(this, lm("Http request %1 succeeded with status code %2")
        .args(m_httpClient->contentLocationUrl(), m_httpClient->lastSysErrorCode()));
    if (m_state == State::LoadingChunks)
    {
        m_state = nextState(m_state);
        m_chunksLoadedAtLeastOnce = true;
        m_wait.wakeAll();
        startTimerForNextRequest(kUpdateChunksDelay); //< Send next request after delay
    }
    else if (m_state == State::updateTimeRange)
    {
        parseTimeRangeData(m_httpClient->fetchMessageBodyBuffer());
        m_state = nextState(m_state);
        m_timeRangeLoadedAtLeastOnce = true;
        m_wait.wakeAll();
        sendRequest(); //< Send next request immediately
    }
}

void HanwhaChunkLoader::startTimerForNextRequest(const std::chrono::milliseconds& delay)
{
    if (m_terminated)
        return;

    m_nextRequestTimerId = nx::utils::TimerManager::instance()->addTimer(
        std::bind(&HanwhaChunkLoader::sendRequest, this),
        delay);
}

void HanwhaChunkLoader::parseTimeRangeData(const QByteArray& data)
{
    qint64 startTimeUsec = AV_NOPTS_VALUE;
    qint64 endTimeUsec = AV_NOPTS_VALUE;
    for (const auto& line: data.split('\n'))
    {
        const auto params = line.trimmed().split('=');
        if (params.size() == 2)
        {
            QByteArray fieldName = params[0];
            QByteArray fieldValue = params[1];

            if (fieldName == kStartTimeParamName)
                startTimeUsec = hanwhaDateTimeToMsec(fieldValue, m_timeZoneShift) * 1000;
            else if (fieldName == kEndTimeParamName)
                endTimeUsec = hanwhaDateTimeToMsec(fieldValue, m_timeZoneShift) * 1000;
        }
    }
    if (startTimeUsec != AV_NOPTS_VALUE && endTimeUsec != AV_NOPTS_VALUE)
    {
        m_startTimeUsec = startTimeUsec;
        m_endTimeUsec = endTimeUsec;
    }
    else
    {
        NX_WARNING(this, lm("Can't parse time range response %1").arg(data));
    }
}

void HanwhaChunkLoader::onGotChunkData()
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

    QList<QByteArray> lines = buffer.split('\n');
    NX_VERBOSE(this, lm("%1 got %2 lines of chunk data")
        .args(m_httpClient->contentLocationUrl(), lines.size()));

    {
        QnMutexLocker lock(&m_mutex);
        const auto currentTimeMs = qnSyncTime->currentMSecsSinceEpoch();
        for (const auto& line: lines)
            parseChunkData(line.trimmed(), currentTimeMs);
    }

    emit gotChunks();
}

bool HanwhaChunkLoader::parseChunkData(const QByteArray& line, qint64 currentTimeMs)
{
    int channelNumberPos = line.indexOf('.') + 1;
    if (channelNumberPos == 0)
        return true; //< Skip header line
    int channelNumberPosEnd = line.indexOf('.', channelNumberPos + 1);
    QByteArray channelNumberData = line.mid(channelNumberPos, channelNumberPosEnd - channelNumberPos);
    int channelNumber = channelNumberData.toInt();

    if (channelNumber > kMaxAllowedChannelNumber)
    {
        NX_WARNING(this, lm("Ignore line %1 due to too big channel number %2 provided")
            .args(line, channelNumber));
        return false;
    }

    if (m_chunks.size() <= channelNumber)
        m_chunks.resize(channelNumber + 1);

    int fieldNamePos = line.lastIndexOf('.') + 1;
    int delimiterPos = line.lastIndexOf('=');
    const QByteArray fieldName = line.mid(fieldNamePos, delimiterPos - fieldNamePos);
    const QByteArray fieldValue = line.mid(delimiterPos + 1).trimmed();

    QnTimePeriodList& chunks = m_chunks[channelNumber];
    if (fieldName == kStartTimeParamName)
    {
        m_lastParsedStartTimeMs = hanwhaDateTimeToMsec(fieldValue, m_timeZoneShift);
    }
    else if (fieldName == kEndTimeParamName)
    {
        auto endTimeMs = hanwhaDateTimeToMsec(fieldValue, m_timeZoneShift);
        if (m_lastParsedStartTimeMs > currentTimeMs)
        {
            NX_DEBUG(this, lm("Ignore period [%1, %2] from future on channel %3")
                .args(m_lastParsedStartTimeMs, endTimeMs, channelNumber));
            return false;
        }

        QnTimePeriod timePeriod(m_lastParsedStartTimeMs, endTimeMs - m_lastParsedStartTimeMs);
        if (m_startTimeUsec == AV_NOPTS_VALUE || chunks.isEmpty())
        {
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

qint64 HanwhaChunkLoader::startTimeUsec(int channelNumber) const
{
    QnMutexLocker lock(&m_mutex);
    const qint64 startTimeMs = m_startTimeUsec / 1000;
    const qint64 endTimeMs = m_endTimeUsec / 1000;
    if (m_chunks.size() <= channelNumber || m_chunks[channelNumber].isEmpty())
        return AV_NOPTS_VALUE;
    return m_chunks[channelNumber].front().startTimeMs * 1000;
}

qint64 HanwhaChunkLoader::endTimeUsec(int channelNumber) const
{
    QnMutexLocker lock(&m_mutex);
    const qint64 startTimeMs = m_startTimeUsec / 1000;
    const qint64 endTimeMs = m_endTimeUsec / 1000;
    if (m_chunks.size() <= channelNumber || m_chunks[channelNumber].isEmpty())
        return AV_NOPTS_VALUE;
    return m_chunks[channelNumber].last().endTimeMs() * 1000;
}

QnTimePeriodList HanwhaChunkLoader::chunks(int channelNumber) const
{
    QnMutexLocker lock(&m_mutex);
    return chunksUnsafe(channelNumber);
}

QnTimePeriodList HanwhaChunkLoader::chunksSync(int channelNumber) const
{
    QnMutexLocker lock(&m_mutex);

    const bool isCameraLoader =
        m_resourceContext->information()->deviceType != kHanwhaNvrDeviceType;

    while ((!m_chunksLoadedAtLeastOnce || (!m_timeRangeLoadedAtLeastOnce && !isCameraLoader))
        && !m_errorOccured)
    {
        m_wait.wait(&m_mutex);
    }

    if (m_errorOccured)
        m_errorOccured = false;

    return chunksUnsafe(channelNumber);
}

QnTimePeriodList HanwhaChunkLoader::chunksUnsafe(int channelNumber) const
{
    const qint64 startTimeMs = m_startTimeUsec / 1000;
    const qint64 endTimeMs = m_endTimeUsec / 1000;
    if (m_chunks.size() <= channelNumber)
        return QnTimePeriodList();

    QnTimePeriod boundingPeriod(startTimeMs, endTimeMs - startTimeMs);

    if (m_resourceContext->information()->deviceType == kHanwhaNvrDeviceType)
        return m_chunks[channelNumber].intersected(boundingPeriod);

    return m_chunks[channelNumber];
}

void HanwhaChunkLoader::setError()
{
    m_errorOccured = true;
    m_chunksLoadedAtLeastOnce = false;
    m_timeRangeLoadedAtLeastOnce = false;
}

HanwhaChunkLoader::State HanwhaChunkLoader::nextState(State currentState) const
{
    if (m_resourceContext->information()->deviceType != kHanwhaNvrDeviceType)
        return State::LoadingChunks;

    if (currentState == State::LoadingChunks)
        return State::updateTimeRange;

    if (currentState == State::updateTimeRange)
        return State::LoadingChunks;

    return State::updateTimeRange;
}

void HanwhaChunkLoader::setTimeZoneShift(std::chrono::seconds timeZoneShift)
{
    m_timeZoneShift = timeZoneShift;
}

std::chrono::seconds HanwhaChunkLoader::timeZoneShift() const
{
    return m_timeZoneShift;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
