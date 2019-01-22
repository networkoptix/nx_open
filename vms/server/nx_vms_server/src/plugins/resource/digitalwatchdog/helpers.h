#pragma once
#if defined(ENABLE_ONNVIF)

#include <nx/vms/server/resource/camera.h>
#include <common/common_globals.h>

class QnDigitalWatchdogResource;

// NOTE: This class uses hardcoded XML reading/writing intentionally, because CPro API may
// reject or crash on requests, which are different from the original.
class CproApiClient
{
public:
    CproApiClient(QnDigitalWatchdogResource* resource);

    boost::optional<QStringList> getSupportedVideoCodecs(Qn::StreamIndex streamIndex);
    boost::optional<QString> getVideoCodec(Qn::StreamIndex streamIndex);

    bool setVideoCodec(Qn::StreamIndex streamIndex, const QString& value);

private:
    bool updateVideoConfig();
    int indexOfStream(Qn::StreamIndex streamIndex);

    boost::optional<std::pair<int, int>> rangeOfTag(
        const QByteArray& openTag, const QByteArray& closeTag,
        int rangeBegin = 0, int rangeSize = 0);

private:
    QnDigitalWatchdogResource* m_resource;
    std::optional<QByteArray> m_videoConfig;
    std::chrono::steady_clock::time_point m_cacheExpiration;
};


/** Some cameras does not support Cpro API and Onvif2, so that class is used in such cases to
 * configure H265 codec.
 */
class JsonApiClient
{
public:
    JsonApiClient(nx::network::SocketAddress address, QAuthenticator auth);

    nx::vms::server::resource::StreamCapabilityMap getSupportedVideoCodecs(Qn::StreamIndex streamIndex);
    bool sendStreamParams(Qn::StreamIndex streamIndex, const QnLiveStreamParams& streamParams);

private:
    QJsonObject getParams(QString paramName);
    QJsonObject setParams(const std::map<QString, QString>& keyValueParams);

    std::optional<QJsonObject> doRequest(const nx::utils::Url &url, QByteArray data);

private:
    nx::network::SocketAddress m_address;
    QAuthenticator m_auth;
};

#endif // defined(ENABLE_ONNVIF)
