#include "url_decorator.h"

#include <plugins/resource/hanwha/hanwha_request_helper.h>

namespace nx::vms_server_plugins::analytics::hanwha {

/*virtual*/ nx::utils::Url CameraValueTransformer::transformUrl(const nx::utils::Url& url)
{
    return url;
}

/*virtual*/ int CameraValueTransformer::transformChannelNumber(int channelNumber)
{
    return channelNumber;
}

/*virtual*/ nx::utils::Url NvrValueTransformer::transformUrl(const nx::utils::Url& url)
{
    return nx::vms::server::plugins::HanwhaRequestHelper::makeBypassUrl(url, m_channelNumber);
}

/*virtual*/ int NvrValueTransformer::transformChannelNumber(int channelNumber)
{
    return 0;
}

} // namespace nx::vms_server_plugins::analytics::hanwha
