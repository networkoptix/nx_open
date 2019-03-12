#include "hanwha_shared_resource_context.h"
#include "hanwha_request_helper.h"
#include "hanwha_resource.h"
#include "hanwha_chunk_loader.h"
#include "hanwha_common.h"

#include <core/resource/abstract_remote_archive_manager.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace vms::server {
namespace plugins {

namespace {

// Limited by NPM-9080VQ, it can only be opening 3 stream at the same time, while it has 4.
static const int kMaxConcurrentRequestNumber = 3;
static const std::chrono::seconds kCacheUrlTimeout(10);
static const std::chrono::seconds kCacheDataTimeout(30);
static const QString kObsoleteInterfaceParameter = lit("Network1");
static const std::chrono::milliseconds kPositionAggregationTimeout(1000);

static const nx::utils::Url cleanUrl(nx::utils::Url url)
{
    url.setPath(QString());
    url.setQuery(QString());
    url.setFragment(QString());
    return url;
}

} // namespace

using namespace nx::core::resource;
using namespace nx::vms::server::resource;

SeekPosition::SeekPosition(qint64 value) : position(value)
{
    timer.restart();
}
bool SeekPosition::canJoinPosition(const SeekPosition& value) const
{
    return
        position != kInvalidPosition
        && value.position == position
        && !timer.hasExpired(kPositionAggregationTimeout);
}

HanwhaSharedResourceContext::HanwhaSharedResourceContext(
    const AbstractSharedResourceContext::SharedId& sharedId)
    :
    information([this]() { return loadInformation(); }, kCacheDataTimeout),
    eventStatuses([this]() { return loadEventStatuses(); }, kCacheDataTimeout),
    videoSources([this]() { return loadVideoSources(); }, kCacheDataTimeout),
    videoProfiles([this]() { return loadVideoProfiles(); }, kCacheDataTimeout),
    videoCodecInfo([this]() { return loadVideoCodecInfo(); }, kCacheDataTimeout),
    isBypassSupported([this]() { return checkBypassSupport(); }, kCacheDataTimeout),
    ptzCalibratedChannels([this]() { return loadPtzCalibratedChannels(); }, kCacheDataTimeout),
    m_sharedId(sharedId),
    m_requestLock(kMaxConcurrentRequestNumber)
{
}

void HanwhaSharedResourceContext::setResourceAccess(
    const nx::utils::Url& url, const QAuthenticator& authenticator)
{
    {
        nx::utils::Url sharedUrl = cleanUrl(url);
        QnMutexLocker lock(&m_dataMutex);
        if (m_resourceUrl == sharedUrl && m_resourceAuthenticator == authenticator)
            return;

        NX_DEBUG(this, "Update resource access: %1 @ %2", authenticator.user(), sharedUrl);
        m_resourceUrl = sharedUrl;
        m_resourceAuthenticator = authenticator;
        m_lastSuccessfulUrlTimer.invalidate();
    }

    information.invalidate();
    eventStatuses.invalidate();
    videoSources.invalidate();
    videoProfiles.invalidate();
}

void HanwhaSharedResourceContext::setLastSucessfulUrl(const nx::utils::Url& value)
{
    QnMutexLocker lock(&m_dataMutex);
    m_lastSuccessfulUrl = cleanUrl(value);
    m_lastSuccessfulUrlTimer.restart();
}

nx::utils::Url HanwhaSharedResourceContext::url() const
{
    QnMutexLocker lock(&m_dataMutex);
    if (!m_lastSuccessfulUrlTimer.hasExpired(kCacheUrlTimeout))
        return m_lastSuccessfulUrl;
    else
        return m_resourceUrl;
}

QAuthenticator HanwhaSharedResourceContext::authenticator() const
{
    QnMutexLocker lock(&m_dataMutex);
    return m_resourceAuthenticator;
}

nx::utils::RwLock* HanwhaSharedResourceContext::requestLock()
{
    return &m_requestLock;
}

void HanwhaSharedResourceContext::startServices()
{
    const auto information = this->information();
    if (!information)
        return;

    {
        QnMutexLocker lock(&m_servicesMutex);
        if (!m_chunkLoader)
            m_chunkLoader = std::make_shared<HanwhaChunkLoader>(this, m_chunkLoaderSettings);
    }

    NX_VERBOSE(this, "Starting services (is NVR: %1)...",
        information->deviceType == HanwhaDeviceType::nvr);

    m_chunkLoader->start(information.value);
}

void HanwhaSharedResourceContext::cleanupUnsafe()
{
    for (const auto& sessionType: m_sessions.keys())
    {
        auto& contextMap = m_sessions[sessionType];
        for (auto itr = contextMap.begin(); itr != contextMap.end();)
        {
            if (itr.value().toStrongRef().isNull())
                itr = contextMap.erase(itr);
            else
                ++itr;
        }
    }
}

int HanwhaSharedResourceContext::totalAmountOfSessions(bool isLive) const
{
    int result = 0;
    for (auto itr = m_sessions.begin(); itr != m_sessions.end(); ++itr)
    {
        bool keyIsLive = itr.key() == HanwhaSessionType::live;
        if (keyIsLive == isLive)
            result += itr.value().size();
    }
    return result;
}

SessionContextPtr HanwhaSharedResourceContext::session(
    HanwhaSessionType sessionType,
    const QnUuid& clientId,
    bool generateNewOne)
{
    if (m_sharedId.isEmpty())
        return SessionContextPtr();

    QnMutexLocker lock(&m_sessionMutex);
    cleanupUnsafe();

    const bool isLive = sessionType == HanwhaSessionType::live;

    auto& sessionsByClientId = m_sessions[sessionType];
    const int maxConsumers = isLive ? kDefaultNvrMaxLiveSessions : m_maxArchiveSessions.load();
    const bool sessionLimitExceeded = !sessionsByClientId.contains(clientId)
        && totalAmountOfSessions(isLive) >= maxConsumers;

    if (sessionLimitExceeded)
        return SessionContextPtr();

    auto strongSessionCtx = sessionsByClientId.value(clientId).toStrongRef();
    if (strongSessionCtx)
        return strongSessionCtx;

    HanwhaRequestHelper helper(shared_from_this());
    const auto response = helper.view(lit("media/sessionkey"));
    if (!response.isSuccessful())
        return SessionContextPtr();

    const auto sessionKey = response.parameter<QString>(lit("SessionKey"));
    if (sessionKey == boost::none)
        return SessionContextPtr();

    strongSessionCtx = SessionContextPtr::create(*sessionKey, clientId);
    sessionsByClientId[clientId] = strongSessionCtx.toWeakRef();

    return strongSessionCtx;
}

OverlappedTimePeriods HanwhaSharedResourceContext::overlappedTimeline(int channelNumber) const
{
    decltype(m_chunkLoader) chunkLoaderCopy;
    {
        QnMutexLocker lock(&m_servicesMutex);
        chunkLoaderCopy = m_chunkLoader;
    }

    if (chunkLoaderCopy)
        return chunkLoaderCopy->overlappedTimeline(channelNumber);

    return OverlappedTimePeriods();
}

OverlappedTimePeriods HanwhaSharedResourceContext::overlappedTimelineSync(
    int channelNumber) const
{
    decltype(m_chunkLoader) chunkLoaderCopy;
    {
        QnMutexLocker lock(&m_servicesMutex);
        chunkLoaderCopy = m_chunkLoader;
    }

    if (chunkLoaderCopy)
        return chunkLoaderCopy->overlappedTimelineSync(channelNumber);

    return OverlappedTimePeriods();
}

qint64 HanwhaSharedResourceContext::timelineStartUs(int channelNumber) const
{
    QnMutexLocker lock(&m_servicesMutex);
    if (m_chunkLoader)
        return m_chunkLoader->startTimeUsec(channelNumber);

    return AV_NOPTS_VALUE;
}

qint64 HanwhaSharedResourceContext::timelineEndUs(int channelNumber) const
{
    QnMutexLocker lock(&m_servicesMutex);
    if (m_chunkLoader)
        return m_chunkLoader->endTimeUsec(channelNumber);

    return AV_NOPTS_VALUE;
}

HanwhaResult<HanwhaInformation> HanwhaSharedResourceContext::loadInformation()
{
    HanwhaInformation info;
    HanwhaRequestHelper helper(shared_from_this());
    info.attributes = helper.fetchAttributes(lit("attributes"));
    if (!info.attributes.isValid())
    {
        return {error(
            info.attributes,
            CameraDiagnostics::CameraInvalidParams(lit("Camera attributes are invalid")))};
    }

    const auto maxArchiveSessionsAttribute = info.attributes.attribute<int>(
        lit("System/MaxSearchSession"));

    if (maxArchiveSessionsAttribute != boost::none)
        m_maxArchiveSessions = maxArchiveSessionsAttribute.get();

    info.cgiParameters = helper.fetchCgiParameters(lit("cgis"));
    if (!info.cgiParameters.isValid())
    {
        return {error(
            info.cgiParameters,
            CameraDiagnostics::CameraInvalidParams(lit("Camera CGI parameters are invalid")))};
    }

    const auto deviceinfo = helper.view(lit("system/deviceinfo"));
    if (!deviceinfo.isSuccessful())
    {
        return {error(deviceinfo,
            CameraDiagnostics::CameraInvalidParams(lit("Can not fetch device information")))};
    }

    HanwhaRequestHelper::Parameters networkRequestParameters;
    const auto interfacesParameter = info.cgiParameters.parameter(
        lit("network/interface/view/InterfaceName"));

    if (interfacesParameter != boost::none)
    {
        const auto possibleValues = interfacesParameter->possibleValues();
        if (!possibleValues.isEmpty())
        {
            networkRequestParameters.emplace(
                lit("InterfaceName"),
                possibleValues.contains(kObsoleteInterfaceParameter)
                    ? kObsoleteInterfaceParameter //< For backward compatibility.
                    : possibleValues.first());
        }
    }

    const auto networkInfo = helper.view("network/interface", networkRequestParameters);
    if (networkInfo.isSuccessful())
    {
        if (const auto value = networkInfo.parameter<QString>("MACAddress"))
            info.macAddress = *value;
    }

    if (info.macAddress.isEmpty())
    {
        if (const auto value = deviceinfo.parameter<QString>("ConnectedMACAddress"))
            info.macAddress = *value;
    }

    if (info.macAddress.isEmpty())
    {
        return {error(deviceinfo,
            CameraDiagnostics::CameraInvalidParams(lit("Can not fetch device MAC address")))};
    }

    if (const auto value = deviceinfo.parameter<QString>("Model"))
        info.model = *value;

    if (const auto value = deviceinfo.parameter<QString>(lit("DeviceType")))
    {
        info.deviceType = QnLexical::deserialized<HanwhaDeviceType>(
            value->trimmed(),
            HanwhaDeviceType::unknown);
    }

    if (const auto value = deviceinfo.parameter<QString>(lit("FirmwareVersion")))
        info.firmware = value->trimmed();

    const auto maxChannels = info.attributes.attribute<int>(lit("System/MaxChannel"));
    info.channelCount = maxChannels.is_initialized() ? *maxChannels : 1;

    return {CameraDiagnostics::NoErrorResult(), std::move(info)};
}

HanwhaResult<HanwhaResponse> HanwhaSharedResourceContext::loadEventStatuses()
{
    HanwhaRequestHelper helper(shared_from_this());
    auto eventStatuses = helper.check(lit("eventstatus/eventstatus"));
    if (!eventStatuses.isSuccessful())
    {
        return {error(
            eventStatuses,
            CameraDiagnostics::RequestFailedResult(
                eventStatuses.requestUrl(), eventStatuses.errorString()))};
    }

    return {CameraDiagnostics::NoErrorResult(), std::move(eventStatuses)};
}

HanwhaResult<HanwhaResponse> HanwhaSharedResourceContext::loadVideoSources()
{
    HanwhaRequestHelper helper(shared_from_this());
    auto videoSources = helper.view(lit("media/videosource"));
    if (!videoSources.isSuccessful())
    {
        return {error(
            videoSources,
            CameraDiagnostics::RequestFailedResult(
                videoSources.requestUrl(),
                videoSources.errorString()))};
    }

    return {CameraDiagnostics::NoErrorResult(), std::move(videoSources)};
}

HanwhaResult<HanwhaResponse> HanwhaSharedResourceContext::loadVideoProfiles()
{
    HanwhaRequestHelper helper(shared_from_this());
    auto videoProfiles = helper.view(lit("media/videoprofile"));
    if (!videoProfiles.isSuccessful()
        && videoProfiles.errorCode() != kHanwhaConfigurationNotFoundError)
    {
        return {CameraDiagnostics::RequestFailedResult(
            videoProfiles.requestUrl(), videoProfiles.errorString())};
    }

    return {CameraDiagnostics::NoErrorResult(), std::move(videoProfiles)};
}

boost::optional<int> HanwhaSharedResourceContext::overlappedId() const
{
    QnMutexLocker lock(&m_servicesMutex);
    if (!m_chunkLoader)
        return boost::none;

    return m_chunkLoader->overlappedId();
}

std::chrono::milliseconds HanwhaSharedResourceContext::timeShift() const
{
    QnMutexLocker lock(&m_servicesMutex);
    return m_chunkLoader ? m_chunkLoader->timeShift() : std::chrono::milliseconds::zero();
}

void HanwhaSharedResourceContext::setTimeShift(std::chrono::milliseconds value)
{
    QnMutexLocker lock(&m_servicesMutex);
    m_chunkLoader->setTimeShift(value);
}

void HanwhaSharedResourceContext::setChunkLoaderSettings(const HanwhaChunkLoaderSettings& settings)
{
    m_chunkLoaderSettings = settings;
}

HanwhaResult<HanwhaCodecInfo> HanwhaSharedResourceContext::loadVideoCodecInfo()
{
    HanwhaRequestHelper helper(shared_from_this());
    helper.setGroupBy(kHanwhaChannelProperty);
    auto response = helper.view(
        lit("media/videocodecinfo"),
        HanwhaRequestHelper::Parameters());

    if (!response.isSuccessful())
    {
        return {CameraDiagnostics::RequestFailedResult(
            response.requestUrl(),
            lit("Request failed"))};
    }

    const auto& info = information();
    if (!info)
        return {info.diagnostics};

    const auto& parameters = info->cgiParameters;
    if (!parameters.isValid())
    {
        return {CameraDiagnostics::CameraInvalidParams(
            lit("Camera CGI parameters are not valid."))};
    }

    HanwhaCodecInfo codecInfo(response, parameters);
    if (!codecInfo.isValid())
        return {CameraDiagnostics::CameraInvalidParams(lit("Video codec info is invalid"))};

    return {CameraDiagnostics::NoErrorResult(), codecInfo};
}

HanwhaResult<bool> HanwhaSharedResourceContext::checkBypassSupport()
{
    const auto& info = information();
    if (!info)
        return {info.diagnostics, false};

    const auto& parameters = info->cgiParameters;
    if (!parameters.isValid())
    {
        return {CameraDiagnostics::CameraInvalidParams(
            lit("Camera CGI parameters are not valid."))};
    }

    const auto bypassParameter = parameters.parameter(lit("bypass/bypass/control/BypassURI"));
    return {CameraDiagnostics::NoErrorResult(), bypassParameter != boost::none};
}

HanwhaResult<std::set<int>> HanwhaSharedResourceContext::loadPtzCalibratedChannels()
{
    HanwhaRequestHelper helper(shared_from_this());
    //helper.setGroupBy(kHanwhaChannelProperty);

    auto response = helper.view(lit("eventrules/internalhandovercalibration"), {});
    if (!response.isSuccessful())
        return {CameraDiagnostics::RequestFailedResult(response.requestUrl(), lit("Failed"))};

    std::set<int> calibratedChannels;
    for (const auto& [key, value]: response.response())
    {
        const auto record = key.split(".");
        if (record.size() < 2)
            continue;

        bool isSuccess = false;
        const auto number = record[1].toInt(&isSuccess);
        if (isSuccess)
            calibratedChannels.insert(number);
    }

    return {CameraDiagnostics::NoErrorResult(), calibratedChannels};
}

qint64 SessionContext::currentPositionUsec() const
{
    QnMutexLocker lock(&m_mutex);
    return m_lastPositionUsec;
}

void SessionContext::updateCurrentPositionUsec(
    qint64 positionUsec,
    bool isForwardPlayback,
    bool force)
{
    QnMutexLocker lock(&m_mutex);
    const bool isGoodPosition = isForwardPlayback
        ? (positionUsec > m_lastPositionUsec)
        : (positionUsec < m_lastPositionUsec);

    if (force || isGoodPosition)
        m_lastPositionUsec = positionUsec;
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
