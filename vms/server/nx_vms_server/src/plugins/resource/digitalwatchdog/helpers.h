#pragma once
#if defined(ENABLE_ONVIF)

#include <nx/vms/server/resource/camera.h>
#include <common/common_globals.h>

class QnDigitalWatchdogResource;

// NOTE: This class uses hardcoded XML reading/writing intentionally, because CPro API may
// reject or crash on requests, which are different from the original.
class CproApiClient
{
public:
    CproApiClient(QnDigitalWatchdogResource* resource);

    boost::optional<QStringList> getSupportedVideoCodecs(nx::vms::api::MotionStreamType streamIndex);
    boost::optional<QString> getVideoCodec(nx::vms::api::MotionStreamType streamIndex);

    bool setVideoCodec(nx::vms::api::MotionStreamType streamIndex, const QString& value);

private:
    bool updateVideoConfig();
    int indexOfStream(nx::vms::api::MotionStreamType streamIndex);

    boost::optional<std::pair<int, int>> rangeOfTag(
        const QByteArray& openTag, const QByteArray& closeTag,
        int rangeBegin = 0, int rangeSize = 0);

private:
    QnDigitalWatchdogResource* m_resource;
    std::optional<QByteArray> m_videoConfig;
    std::chrono::steady_clock::time_point m_cacheExpiration;
};


/** Some cameras does not support Cpro API and Onvif::Media2, so that class is used in such cases to
 * configure H265 codec.
 */
class JsonApiClient
{
public:
    JsonApiClient(nx::network::SocketAddress address, QAuthenticator auth);

    nx::vms::server::resource::StreamCapabilityMap getSupportedVideoCodecs(
        int channelNumber, nx::vms::api::MotionStreamType streamIndex);
    bool sendStreamParams(
        int channelNumber, nx::vms::api::MotionStreamType streamIndex, const QnLiveStreamParams& streamParams);

private:
    QJsonObject getParams(QString paramName);
    QJsonObject setParams(const std::map<QString, QString>& keyValueParams);

    std::optional<QJsonObject> doRequest(const nx::utils::Url &url, QByteArray data);

private:
    nx::network::SocketAddress m_address;
    QAuthenticator m_auth;
};

#endif // defined(ENABLE_ONVIF)
