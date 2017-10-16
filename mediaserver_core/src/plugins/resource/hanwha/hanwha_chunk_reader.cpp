#if defined(ENABLE_HANWHA)

#include <QtCore/QDateTime>
#include <QtCore/QUrlQuery>

#include "hanwha_chunk_reader.h"
#include "utils/common/synctime.h"
#include "core/resource/network_resource.h"
#include <nx/utils/log/log_main.h>
#include "hanwha_request_helper.h"
#include <nx/utils/timer_manager.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

static const int kMaxAllowedChannelNumber = 64;
static const QString kHanwhaDateFormat("yyyy-MM-dd hh:mm:ss");
static const  char kStartTimeParamName[] = "StartTime";
static const  char kEndTimeParamName[] = "EndTime";

static std::chrono::seconds kUpdateChunksDelay(60);
static std::chrono::seconds kResendRequestIfFail(10);

QDateTime parseHanwhaDateTime(const QByteArray& value)
{
    auto dateTime = QDateTime::fromString(value, kHanwhaDateFormat);
    dateTime.setOffsetFromUtc(0);
    return dateTime;
}

HanwhaChunkLoader::HanwhaChunkLoader():
    m_httpClient(new nx_http::AsyncClient())
{
}

HanwhaChunkLoader::~HanwhaChunkLoader()
{
    m_httpClient->pleaseStopSync();
    if (m_nextRequestTimerId)
        nx::utils::TimerManager::instance()->deleteTimer(m_nextRequestTimerId);
}

void HanwhaChunkLoader::setIsCameraLoader(bool isCameraLoader)
{
    m_isCameraLoader = isCameraLoader;
}

void HanwhaChunkLoader::start(const QAuthenticator& auth, const QUrl& cameraUrl, int maxChannels)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (m_state != State::initial)
            return; //< Already started
        m_state = nextState(m_state);
    }
    m_auth = auth;
    m_cameraUrl = cameraUrl;
    m_maxChannels = maxChannels;
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
    m_httpClient->setUserName(m_auth.user());
    m_httpClient->setUserPassword(m_auth.password());

    QUrl loadChunksUrl(m_cameraUrl);

    loadChunksUrl.setPath("/stw-cgi/recording.cgi");
    QUrlQuery query;
    query.addQueryItem("msubmenu", "searchrecordingperiod");
    query.addQueryItem("action", "view");
    query.addQueryItem("Type", "All");
    loadChunksUrl.setQuery(query);

    m_httpClient->setOnDone(
        std::bind(&HanwhaChunkLoader::onHttpClientDone, this));
    m_httpClient->setOnSomeMessageBodyAvailable(nullptr);

    m_httpClient->doGet(loadChunksUrl);
}

qint64 HanwhaChunkLoader::latestChunkTime() const
{
    qint64 result = 0;
    for (const auto& timePeriods: m_chunks)
    {
        if (!timePeriods.empty())
            result = std::max(result, timePeriods.back().startTimeMs);
    }
    return result;
}

void HanwhaChunkLoader::sendLoadChunksRequest()
{
    m_httpClient->setUserName(m_auth.user());
    m_httpClient->setUserPassword(m_auth.password());

    QUrl loadChunksUrl(m_cameraUrl);

    loadChunksUrl.setPath("/stw-cgi/recording.cgi");
    QUrlQuery query;
    query.addQueryItem("msubmenu", "timeline");
    query.addQueryItem("action", "view");
    query.addQueryItem("Type", "All");
    //query.addQueryItem("FromDate", "1970-01-01 00:00:00");
    auto startDateTime = QDateTime::fromMSecsSinceEpoch(latestChunkTime());
    auto endDateTime = QDateTime::fromMSecsSinceEpoch(std::numeric_limits<int>::max() * 1000ll).addDays(-1);
    query.addQueryItem("FromDate", startDateTime.toString(kHanwhaDateFormat));
    query.addQueryItem("ToDate", endDateTime.toString(kHanwhaDateFormat));

    QStringList channelsParam;
    for (int i = 0; i < m_maxChannels; ++i)
        channelsParam << QString::number(i);

    if (!m_isCameraLoader)
        query.addQueryItem("ChannelIdList", channelsParam.join(','));

    loadChunksUrl.setQuery(query);
    m_lastParsedStartTimeMs = AV_NOPTS_VALUE;
    m_httpClient->setOnSomeMessageBodyAvailable(
        std::bind(&HanwhaChunkLoader::onGotChunkData, this));
    m_httpClient->setOnDone(
        std::bind(&HanwhaChunkLoader::onHttpClientDone, this));

    m_httpClient->doGet(loadChunksUrl);
}

void HanwhaChunkLoader::onHttpClientDone()
{
    nx_http::AsyncClient::State state = m_httpClient->state();
    if (state == nx_http::AsyncClient::sFailed)
    {
        NX_WARNING(this, lm("Http request %1 failed with error %2")
            .arg(m_httpClient->url().toString())
            .arg(m_httpClient->lastSysErrorCode()));
        startTimerForNextRequest(kResendRequestIfFail);
        setError();
        m_wait.wakeAll();
        return;
    }
    const int statusCode = m_httpClient->response()->statusLine.statusCode;
    if (statusCode != nx_http::StatusCode::ok)
    {
        NX_WARNING(this, lm("Http request %1 failed with status code %2")
            .arg(m_httpClient->url().toString())
            .arg(statusCode));
        startTimerForNextRequest(kResendRequestIfFail);
        setError();
        m_wait.wakeAll();
        return;
    }

    if (m_state == State::loadingChunks)
    {
        m_state = nextState(m_state);
        m_chunksLoadedAtLeastOnce = true;
        m_wait.wakeAll();
        startTimerForNextRequest(kUpdateChunksDelay); //< Send next request after delay
    }
    else if(m_state == State::updateTimeRange)
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
                startTimeUsec = parseHanwhaDateTime(fieldValue).toMSecsSinceEpoch() * 1000;
            else if (fieldName == kEndTimeParamName)
                endTimeUsec = parseHanwhaDateTime(fieldValue).toMSecsSinceEpoch() * 1000;
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

    auto buffer = m_httpClient->fetchMessageBodyBuffer();
    int index = buffer.lastIndexOf('\n');
    if (index == -1)
    {
        m_unfinishedLine.append(buffer);
        return;
    }

    buffer.insert(0, m_unfinishedLine);

    m_unfinishedLine = QByteArray(buffer.data() + index, buffer.size() - index);
    buffer.truncate(index);

    QList<QByteArray> lines = buffer. split('\n');
    for (const auto& line: lines)
        parseChunkData(line);

    emit gotChunks();
}

bool HanwhaChunkLoader::parseChunkData(const QByteArray& line)
{
    int channelNumberPos = line.indexOf('.') + 1;
    if (channelNumberPos == 0)
        return true; //< Skip header line
    int channelNumberPosEnd = line.indexOf('.', channelNumberPos+1);
    QByteArray channelNumberData = line.mid(channelNumberPos, channelNumberPosEnd - channelNumberPos);
    int channelNumber = channelNumberData.toInt();

    if (channelNumber > kMaxAllowedChannelNumber)
    {
        NX_WARNING(this, lm("Ignore line %1 due to too big channel number %2 provided.").arg(line).arg(channelNumber));
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
        m_lastParsedStartTimeMs = parseHanwhaDateTime(fieldValue).toMSecsSinceEpoch();
    }
    else if (fieldName == kEndTimeParamName)
    {
        auto endTimeMs = parseHanwhaDateTime(fieldValue).toMSecsSinceEpoch();

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
    while ((!m_chunksLoadedAtLeastOnce || (!m_timeRangeLoadedAtLeastOnce && !m_isCameraLoader))
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

    if (!m_isCameraLoader)
    {
        QnTimePeriod boundingPeriod(startTimeMs, endTimeMs - startTimeMs);
        return m_chunks[channelNumber].intersected(boundingPeriod);
    }

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
    if (m_isCameraLoader)
        return State::loadingChunks;

    if (currentState == State::loadingChunks)
        return State::updateTimeRange;

    if (currentState == State::updateTimeRange)
        return State::loadingChunks;

    return State::updateTimeRange;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
