#ifdef ENABLE_ACTI
#include "acti_resource.h"
#include "acti_stream_reader.h"
#include "acti_ptz_controller.h"
#include "acti_audio_transmitter.h"

#include <functional>
#include <memory>

#include <nx_ec/dummy_handler.h>

#include <api/app_server_connection.h>
#include <streaming/mjpeg_stream_reader.h>
#include <utils/common/synctime.h>
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/http/http_client.h>
#include <nx/utils/log/log.h>
#include <core/resource/param.h>
#include "acti_resource_searcher.h"

const QString QnActiResource::MANUFACTURE(lit("ACTI"));
const QString QnActiResource::CAMERA_PARAMETER_GROUP_ENCODER(lit("encoder"));
const QString QnActiResource::CAMERA_PARAMETER_GROUP_SYSTEM(lit("system"));
const QString QnActiResource::CAMERA_PARAMETER_GROUP_DEFAULT(lit("encoder"));
const QString QnActiResource::DEFAULT_ADVANCED_PARAMETERS_TEMPLATE(lit("nx-cube.xml"));
const QString QnActiResource::ADVANCED_PARAMETERS_TEMPLATE_PARAMETER_NAME(lit("advancedParametersTemplate"));

namespace {

const std::chrono::milliseconds kTcpTimeout(8000);
const int DEFAULT_RTSP_PORT = 7070;
int actiEventPort = 0;
int DEFAULT_AVAIL_BITRATE_KBPS[] = { 28, 56, 128, 256, 384, 500, 750, 1000, 1200, 1500, 2000, 2500, 3000, 3500, 4000, 4500, 5000, 5500, 6000 };

const QLatin1String CAMERA_EVENT_ACTIVATED_PARAM_NAME("activated");
const QLatin1String CAMERA_EVENT_DEACTIVATED_PARAM_NAME("deactivated");
const QLatin1String CAMERA_INPUT_NUMBER_PARAM_NAME("di");
const QString kActiDualStreamingMode = lit("dual");
const QString kActiFisheyeStreamingMode = lit("fisheye_view");
const QString kActiCurrentStreamingModeParamName = lit("streaming mode");
const QString kActiGetStreamingModeCapabilitiesParamName = lit("streaming mode cap");
const QString kActiSetStreamingModeCapabilitiesParamName = lit("VIDEO_STREAM");
const QString kActiFirmawareVersionParamName = lit("firmware version");
const QString kActiMacAddressParamName = lit("mac address");
const QString kActiPlatformParamName = lit("platform");
const QString kActiEncoderCapabilitiesParamName = lit("encoder_cap");

const QString kTwoAudioParamName = lit("factory default type");
const QString kTwoWayAudioDeviceType = lit("Two Ways Audio (0x71)");

const char* kApiRequestPath = "/cgi-bin/cmd/";

} // namespace

QnActiResource::QnActiResource(QnMediaServerModule* serverModule):
    nx::vms::server::resource::Camera(serverModule),
    m_desiredTransport(RtspTransport::notDefined),
    m_rtspPort(DEFAULT_RTSP_PORT),
    m_hasAudio(false),
    m_outputCount(0),
    m_inputCount(0),
    m_inputMonitored(false),
    m_advancedParametersProvider(this)
{
    m_audioTransmitter.reset(new ActiAudioTransmitter(this));
    setVendor(lit("ACTI"));

    for (uint i = 0; i < sizeof(DEFAULT_AVAIL_BITRATE_KBPS)/sizeof(int); ++i)
    {
        m_availableBitrates.insert(
            DEFAULT_AVAIL_BITRATE_KBPS[i],
            bitrateToDefaultString(DEFAULT_AVAIL_BITRATE_KBPS[i]));
    }
}

QnActiResource::~QnActiResource()
{
    QnMutexLocker lk( &m_dioMutex );
    for( std::map<quint64, TriggerOutputTask>::iterator
        it = m_triggerOutputTasks.begin();
        it != m_triggerOutputTasks.end();
         )
    {
        const quint64 taskID = it->first;
        m_triggerOutputTasks.erase( it++ );
        lk.unlock();
        nx::utils::TimerManager::instance()->joinAndDeleteTimer( taskID );
        lk.relock();
    }
}

void QnActiResource::checkIfOnlineAsync(std::function<void(bool)> completionHandler)
{
    QString resourceMac = getMAC().toString();
    auto requestCompletionFunc = [resourceMac, completionHandler]
        (SystemError::ErrorCode osErrorCode, int statusCode, nx::network::http::BufferType msgBody,
        nx::network::http::HttpHeaders /*httpHeaders*/) mutable
    {
        if (osErrorCode != SystemError::noError || statusCode != nx::network::http::StatusCode::ok)
            return completionHandler( false );

        if (msgBody.startsWith("ERROR:"))
            return completionHandler( false );

        auto report = QnActiResource::parseSystemInfo( msgBody );
        auto mac = report.value("mac address");
        mac.replace( ':', '-' );
        completionHandler(mac == resourceMac);
    };

    const nx::utils::Url apiUrl =
        createRequestUrl(getAuth(), CAMERA_PARAMETER_GROUP_SYSTEM, "SYSTEM_INFO");
    NX_VERBOSE(this, "Check if online request '%1'.", apiUrl.toString(QUrl::RemoveUserInfo));
    nx::network::http::downloadFileAsync(
        apiUrl, requestCompletionFunc, {}, nx::network::http::AuthType::authDigest);
}

void QnActiResource::setEventPort(int eventPort) {
    actiEventPort = eventPort;
}

int QnActiResource::eventPort() {
    return actiEventPort;
}

QString QnActiResource::getDriverName() const
{
    return MANUFACTURE;
}

QnAbstractStreamDataProvider* QnActiResource::createLiveDataProvider()
{
    return new QnActiStreamReader(toSharedPointer(this));
}

QSize QnActiResource::extractResolution(const QByteArray& resolutionStr) const
{
    QList<QByteArray> params = resolutionStr.split('x');
    if (params.size() < 2 || params[0].isEmpty() || params[1].isEmpty())
        return QSize();
    bool isDigit = params[0].at(0) >= '0' && params[0].at(0) <= '9';
    if (!isDigit)
        params[0] = params[0].mid(1);

    return QSize(params[0].trimmed().toInt(), params[1].trimmed().toInt());
}

QByteArray QnActiResource::unquoteStr(const QByteArray& v)
{
    QByteArray value = v.trimmed();
    int pos1 = value.startsWith('\'') ? 1 : 0;
    int pos2 = value.endsWith('\'') ? 1 : 0;
    return value.mid(pos1, value.length()-pos1-pos2);
}

nx::utils::Url QnActiResource::createRequestUrl(const QAuthenticator& auth, const QString& group,
    const QString& command) const
{
    return nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setHost(getHostAddress())
        .setPort(QUrl(getUrl()).port(nx::network::http::DEFAULT_HTTP_PORT))
        .setUserName(auth.user())
        .setPassword(auth.password())
        .setPath(kApiRequestPath)
        .appendPath(group)
        .setQuery(command);
}

QByteArray QnActiResource::makeActiRequest(
    const QString& group,
    const QString& command,
    nx::network::http::StatusCode::Value& status,
    bool keepAllData,
    QString* const localAddress) const
{
    QByteArray result;
    const auto url = createRequestUrl(getAuth(), group, command);
    status = makeActiRequestByUrl(url, keepAllData, &result, localAddress);
    return result;
}

// NOTE: Not all groups (aka CGI programs) maybe supported by current implementation of request.
nx::network::http::StatusCode::Value QnActiResource::makeActiRequestByUrl(
    const nx::utils::Url& url,
    bool keepAllData,
    QByteArray* const msgBody,
    QString* const localAddress)
{
    const nx::utils::log::Tag logTag = typeid(QnActiResource);

    nx::network::http::HttpClient client;
    client.setAuthType(nx::network::http::AuthType::authBasicAndDigest);
    client.setSendTimeout(kTcpTimeout);
    client.setResponseReadTimeout(kTcpTimeout);

    NX_VERBOSE(logTag, "makeActiRequest: request '%1'.", url);
    const bool result = client.doGet(url);
    if (!result)  //< It seems that HttpClient will log error by itself.
        return nx::network::http::StatusCode::internalServerError;

    auto messageBodyOptional = client.fetchEntireMessageBody();
    if (!messageBodyOptional.has_value())
    {
        NX_DEBUG(logTag, "makeActiRequest: Error getting response body.");
        msgBody->clear();
        return nx::network::http::StatusCode::internalServerError;
    }
    *msgBody = std::move(*messageBodyOptional);

    const auto statusCode(client.response()->statusLine.statusCode);
    if (nx::network::http::StatusCode::isSuccessCode(statusCode))
    {
        if (localAddress)
            *localAddress = client.socket()->getLocalAddress().address.toString();

        // API of camera returns 200 HTTP code on errors, so trying to parse payload here.
        if (msgBody->startsWith("ERROR: bad account."))
            return nx::network::http::StatusCode::unauthorized;
        if (msgBody->startsWith("ERROR: missing USER/PWD."))
            return nx::network::http::StatusCode::unauthorized;
        if (msgBody->startsWith("ERROR:"))  //< NOTE: should be handled somehow?
            NX_DEBUG(logTag, "makeActiRequest: Unhandled error in response: '%1'.", *msgBody);
    }

    if (!keepAllData)
        *msgBody = unquoteStr(msgBody->mid(msgBody->indexOf('=')+1).trimmed());
    return static_cast<nx::network::http::StatusCode::Value>(statusCode);
}

static bool resolutionGreaterThan(const QSize &s1, const QSize &s2)
{
    long long res1 = s1.width() * s1.height();
    long long res2 = s2.width() * s2.height();
    return res1 > res2? true: (res1 == res2 && s1.width() > s2.width()? true: false);
}

QList<QSize> QnActiResource::parseResolutionStr(const QByteArray& resolutions)
{
    QList<QSize> result;
    QList<QSize> availResolutions;

    /*
        Usually ACTi cams resolution response is something like "N1024x768,N1280x1024".
        ACTi-KCM3911 resolution response is "2VIDEO_RESOLUTION_CAP='N2032x1936".
        pureResolutions - is a workaround for such cameras.
    */
    QByteArray pureResolutions = resolutions.split('=').last();
    if (!pureResolutions.isEmpty() && pureResolutions.front() == '\'')
        pureResolutions = pureResolutions.mid(1);

    for(const QByteArray& r: pureResolutions.split(','))
        result << extractResolution(r);
    std::sort(result.begin(), result.end(), resolutionGreaterThan);
    return result;
}

QnActiResource::ActiSystemInfo QnActiResource::parseSystemInfo(const QByteArray& report)
{
    ActiSystemInfo result;
    auto lines = report.split('\n');
    for(const auto& line: lines)
    {
        auto tmp = line.split('=');
        if (tmp.size() != 2)
            continue;

        result.insert(
            QString::fromUtf8(tmp[0]).trimmed().toLower(),
            QString::fromUtf8(tmp[1].trimmed()));
    }

    return result;
}

void QnActiResource::cameraMessageReceived( const QString& path, const QnRequestParamList& /*params*/ )
{
    const QStringList& pathItems = path.split(QLatin1Char('/'));
    if( pathItems.isEmpty() )
        return;

    QString inputNumber;
    bool isActivated = false;
    for( QStringList::const_iterator
        it = pathItems.begin();
        it != pathItems.end();
        ++it )
    {
        const QStringList& tokens = it->split(QLatin1Char('='));
        if( tokens.isEmpty() )
            continue;

        if( tokens[0] == CAMERA_EVENT_ACTIVATED_PARAM_NAME )
            isActivated = true;
        else if( tokens[0] == CAMERA_EVENT_DEACTIVATED_PARAM_NAME )
            isActivated = false;
        else if( tokens.size() > 1 && tokens[0] == CAMERA_INPUT_NUMBER_PARAM_NAME )
            inputNumber = tokens[1];
    }

    emit inputPortStateChanged(
        toSharedPointer(),
        inputNumber,
        isActivated,
        qnSyncTime->currentUSecsSinceEpoch() );
}

QMap<int, QString> QnActiResource::parseVideoBitrateCap(const QByteArray& bitrateCap) const
{
    QMap<int, QString> result;
    int coeff = 1;
    for(auto bitrate: bitrateCap.split(','))
    {
        bitrate = bitrate.trimmed().toUpper();
        if (bitrate.endsWith("M"))
            coeff = 1000;
        else
            coeff = 1;

        result.insert(
            bitrate.left(bitrate.size() - 1).toDouble() * coeff,
            bitrate);
    }

    return result;
}

QString QnActiResource::bitrateToDefaultString(int bitrateKbps) const
{
    const int kKbitInMbit = 1000;
    if (bitrateKbps < kKbitInMbit)
        return lit("%1K").arg(bitrateKbps);

    return lit("%1.%2M").arg(bitrateKbps / 1000).arg((bitrateKbps % 1000) / 100);
}

bool QnActiResource::isRtspAudioSupported(const QByteArray& platform, const QByteArray& firmware) const
{
    QByteArray rtspAudio[][2] =
    {
        {"T",  "4.13"}, // Platform and minimum allowed firmware version
        {"K",  "5.08"},
        {"A1", "6.03"},
        {"D", "6.03"},
        {"E", "6.03"},
        {"B", "6.03"},
        {"I", "6.03"},
        {"AB2L", ""} //< Any firmware version.
    };

    QByteArray version;
    QList<QByteArray> parts = firmware.split('-');
    for (int i = 0; i < parts.size(); ++i)
    {
        if (parts[i].toLower().startsWith('v'))
        {
            version = parts[i].mid(1);
            break;
        }
    }

    for (uint i = 0; i < sizeof(rtspAudio) / sizeof(rtspAudio[0]); ++i)
    {
        if (platform == rtspAudio[i][0])
        {
            const auto minSupportedVersion = rtspAudio[i][1];
            if (version < minSupportedVersion)
            {
                NX_DEBUG(this,
                    lm("RTSP audio is not supported for camera %1. "
                       "Camera firmware %2, platform %3, minimal firmware %4")
                    .args(getPhysicalId(), version, platform, minSupportedVersion));
            }

            return version >= minSupportedVersion;
        }
    }

    NX_DEBUG(this, lm("RTSP audio is not supported for camera %1. Camera firmware %2, platform %3")
        .args(getPhysicalId(), version, platform));
    return false;
}

QString QnActiResource::formatResolutionStr(const QSize& resolution)
{
    return QString(QLatin1String("N%1x%2")).arg(resolution.width()).arg(resolution.height());
}

QString QnActiResource::bestPrimaryCodec() const
{
    return  m_availableEncoders.contains("H264") ? "H264" : "MJPEG";
}

CameraDiagnostics::Result QnActiResource::maxFpsForSecondaryResolution(
    const QString& secondaryCodec,
    const QSize& secondaryResolution,
    int* outFps)
{
    nx::network::http::StatusCode::Value status;
    auto result = makeActiRequest(
        lit("encoder"),
        lit("FPS_CAP_QUERY_ALL=DUAL,%1,%2,%3,%4")
        .arg(bestPrimaryCodec())
        .arg(formatResolutionStr(m_resolutionList[0].last()))
        .arg(secondaryCodec)
        .arg(formatResolutionStr(secondaryResolution)),
        status);

    if (!nx::network::http::StatusCode::isSuccessCode(status))
    {
        return CameraDiagnostics::RequestFailedResult(
            lit("FPS_CAP_QUERY_ALL"),
            "Can't detect max fps for secondary stream");
    }

    QList<QByteArray> stringFps = result.split(';').last().split(',');
    QList<int> fps;
    for (const QByteArray& data: stringFps)
        fps << data.toInt();
    if (fps.isEmpty())
    {
        return CameraDiagnostics::RequestFailedResult(
            lit("FPS_CAP_QUERY_ALL"),
            lit("Empty fps list for resolution %1x%2")
            .arg(secondaryResolution.width(), secondaryResolution.height()));
    }
    std::sort(fps.begin(), fps.end());
    *outFps = fps.last();

    return CameraDiagnostics::NoErrorResult();
}

nx::vms::server::resource::StreamCapabilityMap QnActiResource::getStreamCapabilityMapFromDriver(
    MotionStreamType streamIndex)
{
    using namespace nx::vms::server::resource;

    const auto& resolutionList = m_resolutionList[(int)streamIndex];

    StreamCapabilityMap result;

    for (const auto& codec: m_availableEncoders)
    {
        for (const auto& resolution: resolutionList)
        {
            StreamCapabilityKey key;
            key.codec = codec;
            key.resolution = resolution;

            nx::media::CameraStreamCapability capabilities;
            if (streamIndex == MotionStreamType::secondary)
                capabilities.maxFps = m_maxSecondaryFps[resolution];
            result.insert(key, capabilities);
        }
    }
    return result;
}

CameraDiagnostics::Result QnActiResource::initializeCameraDriver()
{
    nx::network::http::StatusCode::Value status;

    setCameraCapability(Qn::customMediaPortCapability, true);
    updateDefaultAuthIfEmpty(lit("admin"), lit("123456"));

    auto serverReport = makeActiRequest(
        lit("system"),
        lit("SYSTEM_INFO"),
        status,
        true);

    if (nx::network::http::StatusCode::unauthorized == status)
        setStatus(Qn::Unauthorized);

    if (!nx::network::http::StatusCode::isSuccessCode(status))
    {
        return CameraDiagnostics::RequestFailedResult(
            lit("/cgi-bin/system?SYSTEM_INFO"),
            QString::fromUtf8(serverReport));
    }

    auto report = parseSystemInfo(serverReport);

    setFirmware(report.value(kActiFirmawareVersionParamName));
    setMAC(nx::utils::MacAddress(report.value(kActiMacAddressParamName)));

    m_platform = report.value(kActiPlatformParamName)
        .trimmed()
        .toUpper()
        .toLatin1();

    auto encodersStr = report.value(kActiEncoderCapabilitiesParamName);

    if (!encodersStr.isEmpty())
    {
        auto encoders = encodersStr.split(',');
        for (const auto& encoder: encoders)
            m_availableEncoders.insert(toActiEncoderString(encoder.trimmed()));
    }
    else
    {
        //Try to use h264 if no codecs defined;
        m_availableEncoders.insert(lit("H264"));
    }

    auto desiredTransport = resourceData().value<QString>(ResourceDataKey::kDesiredTransport);
    m_desiredTransport = rtspTransportFromString(desiredTransport);

    bool dualStreamingCapability = false;
    bool fisheyeStreamingCapability = false;

    auto streamingModeCapabilities = tryToGetSystemInfoValue(report, kActiGetStreamingModeCapabilitiesParamName);

    if (streamingModeCapabilities)
    {
        auto caps = streamingModeCapabilities.get()
            .toLower()
            .split(',');

        for (const auto& cap: caps)
        {
            if (cap.trimmed() == kActiDualStreamingMode)
                dualStreamingCapability = true;

            if (cap.trimmed() == kActiFisheyeStreamingMode)
                fisheyeStreamingCapability = true;
        }
    }

    bool dualStreaming = report.value("channels").toInt() > 1
        || !report.value("video2_resolution_cap").isEmpty()
        || dualStreamingCapability
        || fisheyeStreamingCapability;

    bool needToSwitchToFisheyeMode = fisheyeStreamingCapability
        && report.contains(kActiCurrentStreamingModeParamName)
        && report.value(kActiCurrentStreamingModeParamName).toLower() != kActiFisheyeStreamingMode;

    bool needToSwitchToDualMode = !fisheyeStreamingCapability
        && dualStreaming
        && report.contains(kActiCurrentStreamingModeParamName)
        && report.value(kActiCurrentStreamingModeParamName).toLower() != kActiDualStreamingMode;

    if (needToSwitchToFisheyeMode)
    {
        makeActiRequest(lit("encoder"), lit("VIDEO_STREAM=FISHEYE_VIEW"), status);

        if (!nx::network::http::StatusCode::isSuccessCode(status))
        {
            auto message =
                lit("Unable to set up fisheye view streaming mode for camera %1, %2")
                .arg(getModel())
                .arg(getUrl());

            NX_DEBUG(this, message);

            return CameraDiagnostics::RequestFailedResult(
                lit("/cgi-bin/encoder?VIDEO_STREAM=FISHEYE_VIEW"),
                message);
        }
    }

    if (needToSwitchToDualMode)
    {
        makeActiRequest(lit("encoder"), lit("VIDEO_STREAM=DUAL"), status);

        if (!nx::network::http::StatusCode::isSuccessCode(status))
        {
            auto message =
                lit("Unable to set up dual streaming mode for camera %1, %2")
                .arg(getModel())
                .arg(getUrl());

            NX_DEBUG(this, message);

            dualStreaming = false;
        }
    }

    // Resolution list depends on streaming mode, so we should make this request
    // after setting proper streaming mode.
    QByteArray resolutions = makeActiRequest(
        lit("system"),
        lit("VIDEO_RESOLUTION_CAP"),
        status);

    // Save this check for backward compatibility
    // since SYSTEM_INFO request potentially can work without auth
    if (nx::network::http::StatusCode::unauthorized == status)
        setStatus(Qn::Unauthorized);

    if (!nx::network::http::StatusCode::isSuccessCode(status))
    {
        return CameraDiagnostics::RequestFailedResult(
            lit("/cgi-bin/encoder?VIDEO_RESOLUTION_CAP"),
            QString::fromUtf8(resolutions));
    }

    auto availResolutions = parseResolutionStr(resolutions);
    if (availResolutions.isEmpty() || availResolutions.isEmpty())
    {
        return CameraDiagnostics::CameraInvalidParams(
            lit("Resolution list is empty"));
    }
    m_resolutionList[(int) MotionStreamType::primary] = availResolutions;

    if (dualStreaming)
    {
        resolutions = makeActiRequest(
            lit("system"),
            lit("CHANNEL=2&VIDEO_RESOLUTION_CAP"),
            status);

        if (!nx::network::http::StatusCode::isSuccessCode(status))
        {
            return CameraDiagnostics::RequestFailedResult(
                lit("CHANNEL=2&VIDEO_RESOLUTION_CAP"),
                QString::fromUtf8(resolutions));
        }

        availResolutions = parseResolutionStr(resolutions);
        int maxSecondaryRes =
            SECONDARY_STREAM_MAX_RESOLUTION.width()
            * SECONDARY_STREAM_MAX_RESOLUTION.height();
        m_resolutionList[(int) MotionStreamType::secondary] = availResolutions;
    }

    // disable extra data aka B2 frames for RTSP (disable value:1, enable: 2)
    auto response = makeActiRequest(
        lit("system"),
        lit("RTP_B2=1"),
        status);

    if (!nx::network::http::StatusCode::isSuccessCode(status))
    {
        return CameraDiagnostics::RequestFailedResult(
            lit("/cgi-bin/system?RTP_B2=1"),
            QString::fromUtf8(response));
    }

    QByteArray fpsString = makeActiRequest(lit("system"), lit("VIDEO_FPS_CAP"), status);

    if (!nx::network::http::StatusCode::isSuccessCode(status))
    {
        return CameraDiagnostics::RequestFailedResult(
            lit("/cgi-bin/system?VIDEO_FPS_CAPS"),
            QString::fromUtf8(fpsString));
    }

    auto fpsList = fpsString.split(';');

    {
        QnMutexLocker lock(&m_mutex);
        for (int i = 0; i < MAX_STREAMS && i < fpsList.size(); ++i)
        {
            QList<QByteArray> fps = fpsList[i].split(',');

            for (const QByteArray& data : fps)
                m_availFps[i] << data.toInt();

            std::sort(m_availFps[i].begin(), m_availFps[i].end());
        }
        setMaxFps(m_availFps[0].last());
    }
    auto rtspPortString = makeActiRequest(lit("system"), lit("V2_PORT_RTSP"), status);

    if (!nx::network::http::StatusCode::isSuccessCode(status))
    {
        return CameraDiagnostics::RequestFailedResult(
            lit("/cgi-bin/system?V2_PORT_RTSP"),
            rtspPortString);
    }

    m_rtspPort = rtspPortString.trimmed().toInt();
    if (m_rtspPort == 0)
        m_rtspPort = DEFAULT_RTSP_PORT;

    m_hasAudio = report.value("audio").toInt() > 0
        && isRtspAudioSupported(m_platform, getFirmware().toUtf8());

    auto bitrateCap = report.value("video_bitrate_cap");
    if (!bitrateCap.isEmpty())
        m_availableBitrates = parseVideoBitrateCap(bitrateCap.toLatin1());

    initializeIO(report);
    initialize2WayAudio(report);

    std::unique_ptr<QnAbstractPtzController> ptzController(
        createPtzControllerInternal());

    fetchAndSetAdvancedParameters();

    setProperty(ResourcePropertyKey::kIsAudioSupported, m_hasAudio ? 1 : 0);

    setProperty(ResourcePropertyKey::kHasDualStreaming, !m_resolutionList[1].isEmpty() ? 1 : 0);
    QString serialNumber = report.value(QnActiResourceSearcher::kSystemInfoProductionIdParamName);
    if (!serialNumber.isEmpty())
        setProperty(QnActiResourceSearcher::kSystemInfoProductionIdParamName, serialNumber);

    auto result = detectMaxFpsForSecondaryCodec();
    if (!result)
        return result;

    saveProperties();

    return CameraDiagnostics::NoErrorResult();
}

CameraDiagnostics::Result QnActiResource::detectMaxFpsForSecondaryCodec()
{
    const auto& resolutionList = m_resolutionList[(int)MotionStreamType::secondary];
    for (const auto& codec: m_availableEncoders)
    {
        for (const auto& resolution: resolutionList)
        {
            int fps = 0;
            auto result = maxFpsForSecondaryResolution(codec, resolution, &fps);
            if (!result)
                return result;
            m_maxSecondaryFps.insert(resolution, fps);
        }
    }
    return CameraDiagnostics::NoErrorResult();
}

bool QnActiResource::SetupAudioInput()
{
    if (!isAudioSupported())
        return true;
    bool value = isAudioEnabled();
    QnMutexLocker lock(&m_audioCfgMutex);
    if (value == m_audioInputOn)
        return true;
    nx::network::http::StatusCode::Value status;
    makeActiRequest(QLatin1String("system"), lit("V2_AUDIO_ENABLED=%1").arg(value ? lit("1") : lit("0")), status);
    if (nx::network::http::StatusCode::isSuccessCode(status)) {
        m_audioInputOn = value;
        return true;
    }
    else {
        return false;
    }
}

void QnActiResource::startInputPortStatesMonitoring()
{
    if( actiEventPort == 0 )
        return;   //no http listener is present

    //considering, that we have excusive access to the camera, so rewriting existing event setup

    //monitoring all input ports
    nx::network::http::StatusCode::Value responseStatusCode = nx::network::http::StatusCode::ok;

        //setting Digital Input Active Level
            //GET /cgi-bin/cmd/encoder?EVENT_DI1&EVENT_DI2&EVENT_RSPDO1&EVENT_RSPDO2 HTTP/1.1\r\n
            //response:
            //EVENT_DI1='0,10'\n
            //EVENT_DI2='0,0'\n
            //EVENT_RSPDO1='1,0'\n
            //ERROR: EVENT_RSPDO2 not found\n

    QString eventStr;
    for( int i = 0; i < m_inputCount; ++i )
    {
        if( !eventStr.isEmpty() )
            eventStr += lit("&");
        eventStr += lit("EVENT_DI%1='0,0'").arg(i+1);
    }
    // Determining address of local interface used to connect to camera.
    QString localInterfaceAddress;
    QByteArray responseMsgBody = makeActiRequest(QLatin1String("encoder"), eventStr,
        responseStatusCode, false, &localInterfaceAddress);
    if (!nx::network::http::StatusCode::isSuccessCode(responseStatusCode))
        return;

    static const int EVENT_HTTP_SERVER_NUMBER = 1;
    static const int MAX_CONNECTION_TIME_SEC = 7;

        //registering itself as event server
            //GET /cgi-bin/cmd/encoder?FTP_SERVER&HTTP_SERVER&SMTP_SEC&SMTP_PRI HTTP/1.1\r\n
            //response:
            //FTP_SERVER=',,,21,0,10000'\n
            //HTTP_SERVER='1,1,192.168.0.101,3451,hz,hz,10'\n
            //HTTP_SERVER='2,0,,80,,,10'\n
            //SMTP_SEC='0,2,,,,,25,110,10'\n

            //GET /cgi-bin/cmd/encoder?HTTP_SERVER=1,1,192.168.0.101,3451,hz,hzhz,10 HTTP/1.1\r\n
            //response:
            //OK: HTTP_SERVER='1,1,192.168.0.101,3451,hz,hzhz,10'\n
    responseMsgBody = makeActiRequest(
        QLatin1String("encoder"),
        lit("HTTP_SERVER=%1,1,%2,%3,guest,guest,%4").arg(EVENT_HTTP_SERVER_NUMBER)
            .arg(localInterfaceAddress).arg(actiEventPort).arg(MAX_CONNECTION_TIME_SEC),
        responseStatusCode);
    if (!nx::network::http::StatusCode::isSuccessCode(responseStatusCode))
        return;

    //registering URL commands (one command per input port)
        //GET /cgi-bin/cmd/encoder?EVENT_RSPCMD1=1,[api/camera_event/98/di/activated],[api/camera_event/98/di/deactivated]&EVENT_RSPCMD2=1,[],[]&EVENT_RSPCMD3=1,[],[]

    const QString cgiPath = lit("/api/camera_event/%1/di").arg(this->getId().toString());
    QString setupURLCommandRequestStr;
    for( int i = 1; i <= m_inputCount; ++i )
    {
        if( !setupURLCommandRequestStr.isEmpty() )
            setupURLCommandRequestStr += QLatin1String("&");
        setupURLCommandRequestStr += lit("EVENT_RSPCMD%1=%2,[%3/%4/di=%1],[%3/%5/di=%1]")
                .arg(i)
                .arg(EVENT_HTTP_SERVER_NUMBER)
                .arg(cgiPath)
                .arg(CAMERA_EVENT_ACTIVATED_PARAM_NAME)
                .arg(CAMERA_EVENT_DEACTIVATED_PARAM_NAME);
    }
    responseMsgBody = makeActiRequest(
        QLatin1String("encoder"),
        setupURLCommandRequestStr,
        responseStatusCode );
    if (!nx::network::http::StatusCode::isSuccessCode(responseStatusCode))
        return;

        //registering events (one event per input port)
            //GET /cgi-bin/cmd/encoder?EVENT_CONFIG HTTP/1.1\r\n
            //response:
            //EVENT_CONFIG='1,1,1234567,00:00,24:00,DI1,MSG1'\n
            //EVENT_CONFIG='2,0,1234567,00:00,24:00,NONE,NONE'\n
            //EVENT_CONFIG='3,0,1234567,00:00,24:00,NONE,NONE'\n

            //GET /cgi-bin/cmd/encoder?EVENT_CONFIG=1,1,1234567,00:00,24:00,DI1,MSG1 HTTP/1.1\r\n
            //response:
            //OK: EVENT_CONFIG='1,1,1234567,00:00,24:00,DI1,MSG1'\n
    QString registerEventRequestStr;
    for( int i = 1; i <= m_inputCount; ++i )
    {
        if( !registerEventRequestStr.isEmpty() )
            registerEventRequestStr += QLatin1String("&");
        registerEventRequestStr += lit("EVENT_CONFIG=%1,1,1234567,00:00,24:00,DI%1,CMD%1").arg(i);
    }
    responseMsgBody = makeActiRequest(
        QLatin1String("encoder"),
        registerEventRequestStr,
        responseStatusCode );
    if (!nx::network::http::StatusCode::isSuccessCode(responseStatusCode))
        return;

    m_inputMonitored = true;
}

void QnActiResource::stopInputPortStatesMonitoring()
{
    if (actiEventPort == 0)
        return;   //< No http listener is present.

    // Unregistering events.
    QString registerEventRequestStr;
    for (int i = 1; i <= m_inputCount; ++i)
    {
        if (!registerEventRequestStr.isEmpty())
            registerEventRequestStr += "&";
        registerEventRequestStr += lit("EVENT_CONFIG=%1,0,1234567,00:00,24:00,DI%1,CMD%1").arg(i);
    }
    m_inputMonitored = false;

    nx::network::http::StatusCode::Value status;
    makeActiRequest("encoder", registerEventRequestStr, status);
    if (!nx::network::http::StatusCode::isSuccessCode(status))
        NX_DEBUG(this, "stopInputPortStatesMonitoring: Unable to stop, HTTP error '%1'.",
            nx::network::http::StatusCode::toString(status));
}

QString QnActiResource::getRtspUrl(int actiChannelNum) const
{
    QUrl url(getUrl());
    url.setScheme(QLatin1String("rtsp"));
    url.setPort(m_rtspPort);
    if (isAudioSupported())
        url.setPath(QString(QLatin1String("/stream%1/")).arg(actiChannelNum));
    else
        url.setPath(QString(QLatin1String("/track%1/")).arg(actiChannelNum));
    return url.toString();
}

QnCameraAdvancedParamValueMap QnActiResource::getApiParameters(const QSet<QString>& ids)
{
    QnCameraAdvancedParamValueList result;
    bool success = true;
    const auto params = getParamsByIds(ids);
    const auto queries = buildGetParamsQueries(params);
    const auto queriesResults = executeParamsQueries(queries, success);
    parseParamsQueriesResult(queriesResults, params, result);
    return result;
}

QSet<QString> QnActiResource::setApiParameters(const QnCameraAdvancedParamValueMap& values)
{
    bool success;
    QSet<QString> idList;
    for(const auto& value: values.toValueList())
        idList.insert(value.id);

    QnCameraAdvancedParamValueList result;
    const auto params = getParamsByIds(idList);
    const auto valueList = values.toValueList();
    const auto queries = buildSetParamsQueries(valueList);
    const auto queriesResults = executeParamsQueries(queries, success);
    parseParamsQueriesResult(queriesResults, params, result);

    const auto maintenanceQueries = buildMaintenanceQueries(valueList);
    if(!maintenanceQueries.empty())
    {
        executeParamsQueries(maintenanceQueries, success);
        return success ? idList : QSet<QString>();
    }

    QSet<QString> resultIds;
    for (const auto value: result)
        resultIds.insert(value.id);

    return resultIds;
}

QString QnActiResource::formatBitrateString(int bitrateKbps) const
{
    if (m_availableBitrates.contains(bitrateKbps))
        return m_availableBitrates[bitrateKbps];

    return bitrateToDefaultString(bitrateKbps);
}

RtspTransport QnActiResource::getDesiredTransport() const
{
    return m_desiredTransport;
}

int QnActiResource::roundFps(int srcFps, Qn::ConnectionRole role) const
{
    QList<int> availFps = (role == Qn::CR_LiveVideo ? m_availFps[0] : m_availFps[1]);
    int minDistance = INT_MAX;
    int result = srcFps;
    for (int i = 0; i < availFps.size(); ++i)
    {
        int distance = qAbs(availFps[i] - srcFps);
        if (distance <= minDistance)
        {
            // Prefer higher fps on the same distance
            minDistance = distance;
            result = availFps[i];
        }
    }

    return result;
}

int QnActiResource::roundBitrate(int srcBitrateKbps) const
{
    int minDistance = INT_MAX;
    int result = srcBitrateKbps;
    for (const auto& bitrate: m_availableBitrates.keys())
    {
        int distance = qAbs(bitrate - srcBitrateKbps);
        if (distance <= minDistance) { // preffer higher bitrate if same distance
            minDistance = distance;
            result = bitrate;
        }
    }

    return result;
}

bool QnActiResource::isAudioSupported() const
{
    return m_hasAudio;
}

bool QnActiResource::hasDualStreamingInternal() const
{
    return getProperty(ResourcePropertyKey::kHasDualStreaming).toInt() > 0;
}

QnAbstractPtzController *QnActiResource::createPtzControllerInternal() const
{
    return new QnActiPtzController(toSharedPointer(this));
}

static const int MIN_DIO_PORT_NUMBER = 1;
static const int MAX_DIO_PORT_NUMBER = 2;

bool QnActiResource::setOutputPortState(
    const QString& ouputID,
    bool activate,
    unsigned int autoResetTimeoutMS )
{
    QnMutexLocker lk( &m_dioMutex );

    bool outputNumberOK = true;
    const int outputNumber = !ouputID.isEmpty() ? ouputID.toInt(&outputNumberOK) : 1;
    if( !outputNumberOK )
        return false;
    if( outputNumber < MIN_DIO_PORT_NUMBER || outputNumber > m_outputCount )
        return false;

    const quint64 timerID = nx::utils::TimerManager::instance()->addTimer(
        this,
        std::chrono::milliseconds::zero());
    m_triggerOutputTasks.insert( std::make_pair( timerID, TriggerOutputTask( outputNumber, activate, autoResetTimeoutMS ) ) );
    return true;
}

void QnActiResource::onTimer( const quint64& timerID )
{
    TriggerOutputTask triggerOutputTask;
    {
        QnMutexLocker lk( &m_dioMutex );
        std::map<quint64, TriggerOutputTask>::iterator it = m_triggerOutputTasks.find( timerID );
        if( it == m_triggerOutputTasks.end() )
            return;

        triggerOutputTask = it->second;
        m_triggerOutputTasks.erase( it );
    }

    const int dioOutputMask =
        1 << (4 + triggerOutputTask.outputID - 1) | //signalling output port id we refer to
        (triggerOutputTask.active ? (1 << (triggerOutputTask.outputID-1)) : 0);        //signalling new port state

    nx::network::http::StatusCode::Value status = nx::network::http::StatusCode::ok;
    QByteArray dioResponse = makeActiRequest(QLatin1String("encoder"),
        lit("DIO_OUTPUT=0x%1").arg(dioOutputMask, 2, 16, QLatin1Char('0')), status);
    if (!nx::network::http::StatusCode::isSuccessCode(status))
        return;

    if (triggerOutputTask.autoResetTimeoutMS > 0)
        m_triggerOutputTasks.insert( std::make_pair(
            nx::utils::TimerManager::instance()->addTimer(
                this, std::chrono::milliseconds(triggerOutputTask.autoResetTimeoutMS)),
            TriggerOutputTask( triggerOutputTask.outputID, !triggerOutputTask.active, 0 ) ) );
}

void QnActiResource::initializeIO( const ActiSystemInfo& systemInfo )
{
    auto it = systemInfo.find( "di" );
    if( it != systemInfo.end() )
        m_inputCount = it.value().toInt();

    it = systemInfo.find( "do" );
    if( it != systemInfo.end() )
        m_outputCount = it.value().toInt();

    QnIOPortDataList ports;
    for( int i = 1; i <= m_outputCount; ++i )
    {
        QnIOPortData value;
        value.portType = Qn::PT_Output;
        value.supportedPortTypes = Qn::PT_Output;
        value.id = QString::number(i);
        value.outputName = tr("Output %1").arg(i);
        ports.push_back(value);
    }

    for( int i = 1; i <= m_inputCount; ++i )
    {
        QnIOPortData value;
        value.portType = Qn::PT_Input;
        value.supportedPortTypes = Qn::PT_Input;
        value.id = QString::number(i);
        value.inputName = tr("Input %1").arg(i);
        ports.push_back(value);
    }
    NX_DEBUG(this, "Initialising %1 input ports and %2 output ports.", m_inputCount, m_outputCount);
    setIoPortDescriptions(std::move(ports), /*needMerge*/ true);
}

/*
 * Operations with maintenance params should be performed when every other param is already changed.
 * Also maintenance params must not get into queries that obtain param values.
 */
bool QnActiResource::isMaintenanceParam(const QnCameraAdvancedParameter &param) const
{
    return param.dataType == QnCameraAdvancedParameter::DataType::Button;
}

QList<QnCameraAdvancedParameter> QnActiResource::getParamsByIds(const QSet<QString>& idList) const
{
    QList<QnCameraAdvancedParameter> params;
    for(const auto& id: idList)
    {
        auto param = m_advancedParametersProvider.getParameterById(id);
        params.append(param);
    }
    return  params;
}

QMap<QString, QnCameraAdvancedParameter> QnActiResource::getParamsMap(const QSet<QString>& idList) const
{
    QMap<QString, QnCameraAdvancedParameter> params;
    for(const auto& id: idList)
        params[id] = m_advancedParametersProvider.getParameterById(id);

    return  params;
}

/*
 * Replaces placeholders in param query with actual values retrieved from camera.
 * Needed when user changes not all params in aggregate parameter.
 * Example: WB_GAIN=127,%WB_R_GAIN becomes WB_GAIN=127,245
 */
QMap<QString, QString> QnActiResource::resolveQueries(QMap<QString, CameraAdvancedParamQueryInfo>& queries) const
{
    nx::network::http::StatusCode::Value status;
    QMap<QString, QString> setQueries;
    QMap<QString, QString> queriesToResolve;

    for (const auto& agregate: queries.keys())
        if(queries[agregate].cmd.indexOf('%') != -1)
            queriesToResolve[queries[agregate].group] += agregate + lit("&");

    QMap<QString, QString> respParams;
    for (const auto& group: queriesToResolve.keys())
    {
        auto response = makeActiRequest(group, queriesToResolve[group], status, true);
        parseCameraParametersResponse(response, respParams);
    }

    for (const auto& agregateName : respParams.keys())
        queries[agregateName].cmd = fillMissingParams(
            queries[agregateName].cmd,
            respParams[agregateName]);

    for (const auto& agregate: queries.keys())
        setQueries[queries[agregate].group] += agregate + lit("=") + queries[agregate].cmd + lit("&");

    return setQueries;
}

/*
 * Used by resolveQueries function. Performs opertaions with parameter strings
 */
QString QnActiResource::fillMissingParams(const QString& unresolvedTemplate,
    const QString& valueFromCamera) const
{
    auto templateValues = unresolvedTemplate.split(",");
    auto cameraValues = valueFromCamera.split(",");

    for (int i = 0; i < templateValues.size(); i++)
    {
        if(i >= cameraValues.size())
            break;
        else if(templateValues.at(i).startsWith("%"))
            templateValues[i] = cameraValues.at(i);
    }

    return templateValues.join(',');
}

boost::optional<QString> QnActiResource::tryToGetSystemInfoValue(const ActiSystemInfo& report,
    const QString& key) const
{
    auto modifiedKey = key;
    if (report.contains(modifiedKey))
        return report.value(modifiedKey);

    modifiedKey.replace(' ', '_');
    if (report.contains(modifiedKey))
        return report.value(modifiedKey);

    modifiedKey.replace('_', ' ');
    if (report.contains(modifiedKey))
        return report.value(modifiedKey);

    return boost::none;
}

QMap<QString, QString> QnActiResource::buildGetParamsQueries(
    const QList<QnCameraAdvancedParameter>& params) const
{
    QSet<QString> agregates;
    QMap<QString, QString> queries;

    for(const auto& param: params)
    {
        if(param.dataType == QnCameraAdvancedParameter::DataType::Button)
            continue;

        auto group = getParamGroup(param);
        auto cmd = getParamCmd(param);

        auto split = cmd.split('=');
        if(split.size() > 1 && !agregates.contains(split[0]))
        {
            queries[group] += split[0] + lit("&");
            agregates.insert(split[0]);
        }
    }

    return queries;
}

QMap<QString, QString> QnActiResource::buildSetParamsQueries(
    const QnCameraAdvancedParamValueList& values) const
{
    QMap<QString, QString> paramToAgregate;
    QMap<QString, QString> paramToValue;
    QMap<QString, CameraAdvancedParamQueryInfo> agregateToCmd;

    for(const auto& val: values)
        paramToValue[val.id] = val.value;

    const auto paramsMap = getParamsMap(paramToValue.keys().toSet());

    for(const auto& id: paramsMap.keys())
    {
        if(isMaintenanceParam(paramsMap[id]))
        {
            paramToValue.remove(id);
            continue;
        }

        auto split = getParamCmd(paramsMap[id]).split('=');

        if(split.size() < 2)
            continue;

        auto agregate = split[0].trimmed();
        auto args = split[1].trimmed();

        paramToAgregate[id] = agregate;

        CameraAdvancedParamQueryInfo info;
        info.group = getParamGroup(paramsMap[id]);
        info.cmd = args;
        agregateToCmd[agregate] = info;
    }

    for(const auto& paramId: paramToValue.keys())
    {
        auto agregate = paramToAgregate[paramId];
        paramToValue[paramId] = convertParamValueToDeviceFormat( paramToValue[paramId], paramsMap[paramId]);
        agregateToCmd[agregate].cmd = agregateToCmd[agregate].cmd
            .replace(lit("%") + paramId, paramToValue[paramId]);
    }
    return resolveQueries(agregateToCmd);
}

QMap<QString, QString> QnActiResource::buildMaintenanceQueries(
    const QnCameraAdvancedParamValueList& values) const
{
    QSet<QString> paramsIds;
    QMap<QString, QString> queries;
    for(const auto& val: values)
        paramsIds.insert(val.id);

    auto params = getParamsByIds(paramsIds);

    for(const auto& param: params)
        if(isMaintenanceParam(param))
            queries[getParamGroup(param)] += param.id + lit("&");

    return queries;
}

/*
 * Executes the queries, parses the response and returns map (paramName => paramValue).
 * paramName and paramValue are raw strings that we got from camera (no parsing of agregate params).
 */
QMap<QString, QString> QnActiResource::executeParamsQueries(const QMap<QString, QString>& queries,
    bool& isSuccessful) const
{
    nx::network::http::StatusCode::Value status;
    QMap<QString, QString> result;
    isSuccessful = true;
    for (const auto& q: queries.keys())
    {
        auto response = makeActiRequest(q, queries[q], status, true);
        if (!nx::network::http::StatusCode::isSuccessCode(status))
            isSuccessful = false;
        parseCameraParametersResponse(response, result);
    }

    return result;
}

/*
 * Retrieves needed params from queries result and put them  to the list.
 * This function performs parsing of agregate params.
 */
void QnActiResource::parseParamsQueriesResult(
    const QMap<QString, QString>& queriesResult,
    const QList<QnCameraAdvancedParameter>& params,
    QnCameraAdvancedParamValueList& result) const
{

    QMap<QString, QString> parsed;
    for(const auto& param: params)
    {
        auto cmd = getParamCmd(param);
        if(!parsed.contains(param.id))
        {
            auto split = cmd.split("=");

            if(split.size() != 2)
                continue;

            auto mainParam = split[0].trimmed();
            auto mask = split[1].trimmed();
            if(queriesResult.contains(mainParam))
                extractParamValues(queriesResult[mainParam], mask, parsed);
        }

        if(parsed.contains(param.id))
        {
            parsed[param.id] = convertParamValueFromDeviceFormat(parsed[param.id], param);
            result.append(QnCameraAdvancedParamValue(param.id, parsed[param.id]));
        }
    }
}

QString QnActiResource::getParamCmd(const QnCameraAdvancedParameter& param) const
{
    return param.writeCmd.isEmpty() ?
        param.id + lit("=%") + param.id :
        param.writeCmd;

}

/*
 * Extracts params from real string from camera with given mask.
 * Example: paramValue = "1,2,3", mask="%param1,%param2".
 * Two items will be added to map: param1 => "1" , param2 => "2,3"
 */
void QnActiResource::extractParamValues(const QString& paramValue, const QString& mask,
    QMap<QString, QString>& result) const
{

    const auto paramNames = mask.split(',');
    const auto paramValues = paramValue.split(',');
    const auto paramCount = paramNames.size();

    for (int i = 0; i < paramCount; i++)
    {
        auto name = paramNames.at(i).mid(1);
        if(i < paramCount - 1)
            result[name] = paramValues.at(i);
        else
            result[name] = paramValues.mid(i).join(',');
    }
}

QString QnActiResource::getParamGroup(const QnCameraAdvancedParameter& param) const
{
    return param.readCmd.isEmpty() ? CAMERA_PARAMETER_GROUP_DEFAULT : param.readCmd;
}

bool QnActiResource::parseParameter(const QString& paramStr, QnCameraAdvancedParamValue& value) const
{
    bool success = false;
    auto split = paramStr.trimmed().split('=');

    if(split.size() == 2)
    {
        success = true;
        if(split[0].startsWith("OK:"))
            value.id = split[0].split(' ')[1].trimmed();
        else
            value.id = split[0];

        value.value = split[1].mid(1, split[1].size()-2);
    }

    return success;
}

void QnActiResource::parseCameraParametersResponse(const QByteArray& response,
    QnCameraAdvancedParamValueList& result) const
{
    auto lines = response.split('\n');
    for(const auto& line: lines)
    {
        if(line.startsWith("ERROR:"))
            continue;
        QnCameraAdvancedParamValue param;
        if(parseParameter(line, param))
            result.append(param);
    }
}

void QnActiResource::parseCameraParametersResponse(const QByteArray& response,
    QMap<QString, QString>& result) const
{
    auto lines = response.split('\n');
    for(const auto& line: lines)
    {
        if(line.startsWith("ERROR:"))
            continue;
        QnCameraAdvancedParamValue param;
        if(parseParameter(line, param))
            result[param.id] = param.value;
    }
}

QString QnActiResource::convertParamValueToDeviceFormat(const QString& paramValue,
    const QnCameraAdvancedParameter& param) const
{
    auto tmp = paramValue;

    if(param.dataType == QnCameraAdvancedParameter::DataType::Enumeration)
        tmp = param.toInternalRange(paramValue).replace('|', ',');
    else if(param.dataType == QnCameraAdvancedParameter::DataType::Bool)
        tmp = (paramValue == lit("true") ?  lit("1") : lit("0"));

    return tmp;
}

QString QnActiResource::convertParamValueFromDeviceFormat(const QString& paramValue,
    const QnCameraAdvancedParameter& param) const
{
    QString tmp = paramValue;
    if(param.dataType == QnCameraAdvancedParameter::DataType::Enumeration )
    {
        tmp = tmp.replace(',', '|');
        tmp = param.fromInternalRange(
            param.fromInternalRange(tmp).isEmpty() ? paramValue : tmp);
    }
    else if(param.dataType == QnCameraAdvancedParameter::DataType::Bool)
    {
        tmp = (paramValue == lit("1") ?  lit("true") : lit("false"));
    }

    return tmp;
}

bool QnActiResource::loadAdvancedParametersTemplateFromFile(QnCameraAdvancedParams& params,
    const QString& templateFilename)
{
    QFile paramsTemplateFile(templateFilename);
#ifdef _DEBUG
    QnCameraAdvacedParamsXmlParser::validateXml(&paramsTemplateFile);
#endif

    bool result = QnCameraAdvacedParamsXmlParser::readXml(&paramsTemplateFile, params);
    if (!result)
    {
        NX_DEBUG(this, lit("Error while parsing xml (acti) %1").arg(templateFilename));
    }

    return result;
}

void QnActiResource::fetchAndSetAdvancedParameters()
{
    m_advancedParametersProvider.clear();

    auto templateFile = getAdvancedParametersTemplate();
    QnCameraAdvancedParams params;
    if (!loadAdvancedParametersTemplateFromFile(
            params,
            lit(":/camera_advanced_params/") + templateFile))
        return;

    QSet<QString> supportedParams = calculateSupportedAdvancedParameters(params);
    m_advancedParametersProvider.assign(params.filtered(supportedParams));
}

QString QnActiResource::getAdvancedParametersTemplate() const
{
    auto advancedParametersTemplate = resourceData().value<QString>(ADVANCED_PARAMETERS_TEMPLATE_PARAMETER_NAME);
    return advancedParametersTemplate.isEmpty() ?
        DEFAULT_ADVANCED_PARAMETERS_TEMPLATE : advancedParametersTemplate;
}

QSet<QString> QnActiResource::calculateSupportedAdvancedParameters(
    const QnCameraAdvancedParams& allParams) const
{
    QList<QnCameraAdvancedParameter> paramsList;
    QSet<QString> result;
    auto paramIds = allParams.allParameterIds();
    bool success = true;

    for(const auto& id: paramIds)
    {
        auto param = allParams.getParameterById(id);
        paramsList.append(param);
        if(param.dataType == QnCameraAdvancedParameter::DataType::Button)
            result.insert(param.id);
    }

    auto queries = buildGetParamsQueries(paramsList);
    auto queriesResult = executeParamsQueries(queries, success);

    for(const auto& param: paramsList)
    {
        if(queriesResult.contains(param.id))
            result.insert(param.id);
        else
        {
            auto cmd = getParamCmd(param);
            auto agregate = cmd.split('=')[0];
            if(queriesResult.contains(agregate))
                result.insert(param.id);
        }
    }

    return result;
}

void QnActiResource::initialize2WayAudio(const ActiSystemInfo& systemInfo)
{
    if (!systemInfo.contains(kTwoAudioParamName))
        return;

    if (systemInfo[kTwoAudioParamName] == kTwoWayAudioDeviceType)
        setCameraCapabilities(getCameraCapabilities() | Qn::AudioTransmitCapability);
}

std::vector<nx::vms::server::resource::Camera::AdvancedParametersProvider*>
    QnActiResource::advancedParametersProviders()
{
    return {&m_advancedParametersProvider};
}

QString QnActiResource::toActiEncoderString(const QString& value)
{
    if (value.contains(lit("JPEG")))
        return lit("MJPEG");
    return "H264"; //< Default value.
}

#endif // #ifdef ENABLE_ACTI
