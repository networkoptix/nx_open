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
static const QByteArray kStartTimeParamName("StartTime");
static const QByteArray kEndTimeParamName("EndTime");

static std::chrono::seconds kUpdateChunksDelay(60);
static std::chrono::seconds kResendRequestIfFail(10);

static const QString kDateTimeFormat = lit("yyyy-MM-dd hh:mm:ss");

static QDateTime kMinDateTime = QDateTime::fromString(
    lit("2000-01-01 00:00:00"),
    kDateTimeFormat);

static QDateTime kMaxDateTime = QDateTime::fromString(
    lit("2037-12-31 00:00:00"),
    kDateTimeFormat);

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

void HanwhaChunkLoader::sendRequest()
{
    switch (m_state)
    {
        case State::updateTimeRange:
            sendUpdateTimeRangeRequest();
            break;
        case State::loadingChunks:
            sendLoadChunksRequest();
            break;
        default:
            break;
    }
}

void HanwhaChunkLoader::sendUpdateTimeRangeRequest()
{
    prepareHttpClient();

    // TODO: Check for 'attributes/Recording/Support/SearchPeriod' and use only if it's supported,
    // otherwise we have to load all periods constantly and clean them up by timeout.
    const auto loadChunksUrl = buildUrl(
        lit("recording/searchrecordingperiod/view"),
        {
            {lit("Type"), lit("All")},
            {lit("ResultsInUTC"), m_isUtcEnabled ? kHanwhaTrue : kHanwhaFalse}
        });

    m_httpClient->setOnDone([this](){onHttpClientDone();});
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
    prepareHttpClient();

    {
        QnMutexLocker lock(&m_mutex);
        m_newChunks.clear();
    }

    auto updateLagMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        kUpdateChunksDelay).count();

    const auto timeZoneShift = m_isUtcEnabled
        ? std::chrono::seconds::zero()
        : std::chrono::seconds(m_timeZoneShift);

    auto startDateTime = hasBounds()
        ? toHanwhaDateTime(latestChunkTimeMs() - updateLagMs, timeZoneShift)
        : kMinDateTime;

    auto endDateTime = hasBounds()
        ? QDateTime::fromMSecsSinceEpoch(std::numeric_limits<int>::max() * 1000ll).addDays(-1)
        : kMaxDateTime;

    if (!hasBounds())
    {
        m_startTimeUsec = AV_NOPTS_VALUE;
        m_endTimeUsec = AV_NOPTS_VALUE;
    }

    using P = std::pair<QString, QString>;
    std::map<QString, QString> parameters = {
        P(lit("Type"), lit("All")),
        P(lit("FromDate"), convertDateToString(startDateTime)),
        P(lit("ToDate"), convertDateToString(endDateTime))
    };

    if (m_isNvr)
    {
        QStringList channelsParam;
        for (int i = 0; i < m_maxChannels; ++i)
            channelsParam << QString::number(i);

        parameters.emplace(lit("ChannelIDList"), channelsParam.join(','));
    }

    const auto loadChunksUrl = buildUrl(
        lit("recording/timeline/view"),
        parameters);

    m_lastParsedStartTimeMs = AV_NOPTS_VALUE;
    m_httpClient->setOnSomeMessageBodyAvailable([this](){onGotChunkData();});
    m_httpClient->setOnDone([this](){onHttpClientDone();});

    // TODO: Use m_resourceConext->requestSemaphore().
    m_httpClient->doGet(loadChunksUrl);
}

void HanwhaChunkLoader::onHttpClientDone()
{
    if (m_httpClient->state() == nx_http::AsyncClient::State::sFailed)
    {
        NX_WARNING(this, lm("Http request %1 failed with error %2")
            .args(m_httpClient->contentLocationUrl(), m_httpClient->lastSysErrorCode()));

        setError();
        m_wait.wakeAll();
        startTimerForNextRequest(kResendRequestIfFail);
        return;
    }

    const auto statusCode = m_httpClient->response()->statusLine.statusCode;
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

    if (m_state == State::loadingChunks)
    {
        {
            QnMutexLocker lock(&m_mutex);
            if (!hasBounds())
                m_chunks.swap(m_newChunks);
            m_errorOccured = false;
            m_chunksLoadedAtLeastOnce = true;
        }

        m_state = nextState(m_state);
        m_wait.wakeAll();
        startTimerForNextRequest(kUpdateChunksDelay); //< Send next request after delay
    }
    else if (m_state == State::updateTimeRange)
    {
        parseTimeRangeData(m_httpClient->fetchMessageBodyBuffer());
        m_timeRangeLoadedAtLeastOnce = true;
        m_errorOccured = false;
        m_state = nextState(m_state);
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
    for (const auto& line: data.split(L'\n'))
    {
        const auto params = line.trimmed().split(L'=');
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
        const auto startTimeMs = m_startTimeUsec / 1000;

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
    const auto channelNumberPos = line.indexOf(L'.') + 1;
    if (channelNumberPos == 0)
        return true; //< Skip header line

    const auto channelNumberPosEnd = line.indexOf(L'.', channelNumberPos + 1);
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
        if (m_startTimeUsec == AV_NOPTS_VALUE || chunks.isEmpty())
        {
            chunks.push_back(timePeriod);
            m_startTimeUsec = timePeriod.startTimeMs * 1000;
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
    if (m_chunks.size() <= channelNumber || m_chunks[channelNumber].isEmpty())
        return AV_NOPTS_VALUE;

    auto result = m_chunks[channelNumber].front().startTimeMs * 1000;
    if (m_startTimeUsec != AV_NOPTS_VALUE)
        result = std::max(m_startTimeUsec, result);

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
    return chunksUnsafe(channelNumber);
}

QnTimePeriodList HanwhaChunkLoader::chunksSync(int channelNumber) const
{
    QnMutexLocker lock(&m_mutex);

    while (!m_chunksLoadedAtLeastOnce && !m_errorOccured)
        m_wait.wait(&m_mutex);

    return chunksUnsafe(channelNumber);
}

QnTimePeriodList HanwhaChunkLoader::chunksUnsafe(int channelNumber) const
{
    const qint64 startTimeMs = m_startTimeUsec / 1000;
    const qint64 endTimeMs = m_endTimeUsec / 1000;
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
    m_chunksLoadedAtLeastOnce = false;
    m_timeRangeLoadedAtLeastOnce = false;
}

HanwhaChunkLoader::State HanwhaChunkLoader::nextState(State currentState) const
{
    if (!hasBounds())
        return State::loadingChunks;

    if (currentState == State::updateTimeRange)
        return State::loadingChunks;

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

nx::utils::Url HanwhaChunkLoader::buildUrl(
    const QString& path,
    std::map<QString, QString> parameters) const
{
    const auto split = path.split(L'/');
    if (split.size() != 3)
        return nx::utils::Url();

    auto url = m_resourceContext->url();
    url.setPath(lit("/stw-cgi/%1.cgi").arg(split[0].trimmed()));

    QUrlQuery query;
    query.addQueryItem(lit("msubmenu"), split[1].trimmed());
    query.addQueryItem(lit("action"), split[2].trimmed());
    for (const auto& parameter : parameters)
        query.addQueryItem(parameter.first, parameter.second);

    url.setQuery(query);
    return url;
}

void HanwhaChunkLoader::prepareHttpClient()
{
    const auto authenticator = m_resourceContext->authenticator();
    QnMutexLocker lock(&m_mutex);
    m_httpClient->setUserName(authenticator.user());
    m_httpClient->setUserPassword(authenticator.password());
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
