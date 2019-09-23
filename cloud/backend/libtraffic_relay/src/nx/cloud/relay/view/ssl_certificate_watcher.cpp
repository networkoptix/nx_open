#include "ssl_certificate_watcher.h"

#include <nx/utils/log/log_main.h>

#include "../relay_service.h"
#include "../settings.h"

namespace nx::cloud::relay::view {

SslCertificateWatcher::SslCertificateWatcher(RelayService* service, const conf::Https& settings):
	m_service(service),
	m_settings(settings),
	m_timer(std::make_unique<nx::network::aio::Timer>())
{
	m_timer->bindToAioThread(getAioThread());
	startTimer(std::chrono::milliseconds(0));
}

SslCertificateWatcher::~SslCertificateWatcher()
{
	pleaseStopSync();
}

void SslCertificateWatcher::stopWhileInAioThread()
{
	m_timer.reset();
}

void SslCertificateWatcher::startTimer(std::chrono::milliseconds timeout)
{
	m_timer->start(
		timeout,
		[this]()
		{
			std::ifstream file(m_settings.certificatePath);
			if (!file)
			{
				NX_WARNING(this, "Failed to open ssl certificate file %1",
					m_settings.certificatePath);
				return startTimer(m_settings.certificateMonitorTimeout);
			}

			// Reading entire contents of the file
			std::string fileContents{
				std::istreambuf_iterator<char>(file),
				std::istreambuf_iterator<char>()};

			if (m_fileContents.empty())
			{
				m_fileContents = std::move(fileContents);
				return startTimer(m_settings.certificateMonitorTimeout);
			}

			if (m_fileContents == fileContents)
				return startTimer(m_settings.certificateMonitorTimeout);

			NX_INFO(this, "Ssl certificate file changed, restarting service.");

			m_service->restart();
		});
}

} // namespace nx::cloud::relay::view