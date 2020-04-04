#include "url_decorator.h"

#include <plugins/resource/hanwha/hanwha_request_helper.h>

namespace nx::vms_server_plugins::analytics::hanwha {

/*virtual*/ nx::utils::Url MockUrlDecorator::decorate(const nx::utils::Url& url)
{
	return url;
}

/*virtual*/ nx::utils::Url BypassUrlDecorator::decorate(const nx::utils::Url& url)
{
	return nx::vms::server::plugins::HanwhaRequestHelper::makeBypassUrl(url, m_channelNumber);
}

} // namespace nx::vms_server_plugins::analytics::hanwha
