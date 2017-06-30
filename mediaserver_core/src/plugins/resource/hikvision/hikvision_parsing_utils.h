#include <vector>

#include <nx/network/http/http_types.h>
#include <utils/media/audioformat.h>

namespace nx {
namespace plugins {
namespace hikvision {

struct ChannelStatusResponse final
{
    QString id;
    bool enabled = false;
    QString audioCompression;
    QString audioInputType;
    int speakerVolume = 0;
    bool noiseReduce = false;
    int sampleRateKHz = 0;
};

struct OpenChannelResponse final
{
    QString sessionId;
};

struct CommonResponse final
{
    QString statusCode;
    QString statusString;
    QString subStatusCode;
};

QnAudioFormat toAudioFormat(const QString& codecName, int sampleRateKHz);

std::vector<ChannelStatusResponse> parseAvailableChannelsResponse(
    nx_http::StringType message);

boost::optional<ChannelStatusResponse> parseChannelStatusResponse(
    nx_http::StringType message);

boost::optional<OpenChannelResponse> parseOpenChannelResponse(
    nx_http::StringType message);

boost::optional<CommonResponse> parseCommonResponse(
    nx_http::StringType message);

} // namespace hikvision
} // namespace plugins
} // namespace nx