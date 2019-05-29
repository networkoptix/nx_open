#pragma once

#include <nx/cloud/mediator/relay/relay_cluster_client.h>
#include <nx/cloud/mediator/relay/relay_cluster_client_factory.h>
#include <nx/geo_ip/abstract_resolver.h>

namespace nx::hpm::test {

/**
* For test purposes, overrides OnlineRelaysClusterClient implementation of
* AbstractRelayClusterClient with one
* that does not throw an exception when there is no Geolite2-*.mmdb database file.
*/
class OverrideRelayClusterClientFactory
{
public:
	OverrideRelayClusterClientFactory()
	{
		m_relayClusterClientFactoryFuncBak =
			nx::hpm::RelayClusterClientFactory::instance().setCustomFunc(
				[](const nx::hpm::conf::Settings& settings,
                   nx::geo_ip::AbstractResolver* /*resolver*/)
				{
					return std::make_unique<nx::hpm::RelayClusterClient>(settings);
				});
	}

	~OverrideRelayClusterClientFactory()
	{
		if (m_relayClusterClientFactoryFuncBak)
		{
			nx::hpm::RelayClusterClientFactory::instance().setCustomFunc(
				std::move(m_relayClusterClientFactoryFuncBak));
		}
	}

private:
	nx::utils::MoveOnlyFunc<
		nx::hpm::RelayClusterClientFactoryFunction> m_relayClusterClientFactoryFuncBak;
};

} // namespace nx::hpm::test
