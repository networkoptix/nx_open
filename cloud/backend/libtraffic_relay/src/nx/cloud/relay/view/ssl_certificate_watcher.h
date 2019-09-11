#pragma once

#include <nx/network/aio/repetitive_timer.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::cloud::relay {

class RelayService;
namespace conf { struct Https; }

namespace view {

class SslCertificateWatcher :
	public nx::network::aio::BasicPollable
{
	using base_type = nx::network::aio::BasicPollable;
public:
	/**
	 * Restarts the RelayService if the contents of certificatePath in settings changes.
	 */
	SslCertificateWatcher(RelayService* service, const conf::Https& settings);
	~SslCertificateWatcher();

protected:
	virtual void stopWhileInAioThread() override;

	void startTimer(std::chrono::milliseconds timeout);

private:
	RelayService* m_service;
	const conf::Https& m_settings;
	std::unique_ptr<nx::network::aio::Timer> m_timer;
	std::string m_fileContents;
};

} // namespace view
} // namespace nx::cloud::relay