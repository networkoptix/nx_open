#pragma once

#include <common/common_globals.h>

class QnDigitalWatchdogResource;

// NOTE: This class uses hardcoded XML reading/writing intentionally, because CPro API may
// reject or crash on requests, which are different from the original.
class CproApiClient
{
public:
    CproApiClient(QnDigitalWatchdogResource* resource);

    bool updateVideoConfig();

    boost::optional<QStringList> getSupportedVideoCodecs(Qn::StreamIndex streamIndex);
    boost::optional<QString> getVideoCodec(Qn::StreamIndex streamIndex);

    bool setVideoCodec(Qn::StreamIndex streamIndex, const QString& value);

private:
    int indexOfStream(Qn::StreamIndex streamIndex);

    boost::optional<std::pair<int, int>> rangeOfTag(
        const QByteArray& openTag, const QByteArray& closeTag,
        int rangeBegin = 0, int rangeSize = 0);

private:
    QnDigitalWatchdogResource* m_resource;
    std::optional<QByteArray> m_videoConfig;
    std::chrono::steady_clock::time_point m_cacheExpiration;
};
