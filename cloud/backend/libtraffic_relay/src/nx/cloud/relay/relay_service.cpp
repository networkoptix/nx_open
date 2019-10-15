#include "relay_service.h"

#include <nx/utils/system_error.h>

#include "controller/controller.h"
#include "libtraffic_relay_app_info.h"
#include "model/model.h"
#include "model/remote_relay_peer_pool.h"
#include "settings.h"
#include "statistics_provider.h"
#include "view/view.h"

namespace nx {
namespace cloud {
namespace relay {

RelayService::RelayService(int argc, char **argv):
    base_type(argc, argv, AppInfo::applicationDisplayName())
{
}

std::vector<network::SocketAddress> RelayService::httpEndpoints() const
{
    return m_view->httpEndpoints();
}

std::vector<network::SocketAddress> RelayService::httpsEndpoints() const
{
    return m_view->httpsEndpoints();
}

const relaying::AbstractListeningPeerPool& RelayService::listeningPeerPool() const
{
    return m_model->listeningPeerPool();
}

const model::AbstractRemoteRelayPeerPool& RelayService::remoteRelayPeerPool() const
{
    return m_model->remoteRelayPeerPool();
}

std::unique_ptr<utils::AbstractServiceSettings> RelayService::createSettings()
{
    return std::make_unique<conf::Settings>();
}

int RelayService::serviceMain(const utils::AbstractServiceSettings& abstractSettings)
{
    const conf::Settings& settings = static_cast<const conf::Settings&>(abstractSettings);

	//watchSslCertificateFileIfNeeded(settings);

    auto model = std::make_unique<Model>(settings);
    m_model = model.get();

    while (!m_model->doMandatoryInitialization())
    {
        if (isTerminated())
            return -1;

        NX_INFO(this, lm("Retrying model initialization after delay"));
        std::this_thread::sleep_for(settings.listeningPeerDb().connectionRetryDelay);
    }

    auto controller = std::make_unique<Controller>(settings, model.get());

    View view(settings, model.get(), controller.get());
    m_view = &view;

    const auto remoteRelayPeerPool =
        dynamic_cast<const model::RemoteRelayPeerPool*>(&model->remoteRelayPeerPool());

    auto statisticsProvider = StatisticsProviderFactory::instance().create(
        model->listeningPeerPool(),
        view.httpServer(),
        controller->trafficRelay(),
        remoteRelayPeerPool && remoteRelayPeerPool->peerDb()
            ? &remoteRelayPeerPool->peerDb()->synchronizationEngine().statisticsProvider()
            : nullptr,
        remoteRelayPeerPool && remoteRelayPeerPool->sqlQueryExecutor()
            ? &remoteRelayPeerPool->sqlQueryExecutor()->statisticsCollector()
            : nullptr);
    view.registerStatisticsApiHandlers(*statisticsProvider);

    if (!registerThisInstanceNameInCluster(settings, controller.get()))
        return -1;

    // TODO: #ak: process rights reduction should be done here.

    view.start();

    int result = runMainLoop();

	if (m_fileWatcher)
		m_fileWatcher.reset();

    // Stop view (stop listening and close all connections)
    // to ensure there will be no new requests and, thus, view will not initiate
    // new requests to the controller.
    view.stop();

    controller.reset();

    model.reset();

    return result;
}

bool RelayService::registerThisInstanceNameInCluster(
    const conf::Settings& settings,
    Controller* controller)
{
    nx::utils::Url publicUrl;
    if (settings.server().name.empty())
    {
        const auto publicIp = controller->discoverPublicAddress();
        if (!publicIp)
        {
            NX_ERROR(this, "Failed to discover public address. Terminating.");
            return false;
        }
        publicUrl.setHost(publicIp->toString());

        if (!m_view->httpEndpoints().empty())
        {
            publicUrl.setPort(m_view->httpEndpoints().front().port);
            publicUrl.setScheme(nx::network::http::kUrlSchemeName);
        }
        else
        {
            publicUrl.setPort(m_view->httpsEndpoints().front().port);
            publicUrl.setScheme(nx::network::http::kSecureUrlSchemeName);
        }
    }
    else
    {
        publicUrl.setHost(settings.server().name.c_str());
        if (!m_view->httpEndpoints().empty())
            publicUrl.setScheme(nx::network::http::kUrlSchemeName);
        else
            publicUrl.setScheme(nx::network::http::kSecureUrlSchemeName);
    }

    m_model->remoteRelayPeerPool().registerHttpApi(&m_view->messageDispatcher());
    m_model->remoteRelayPeerPool().setPublicUrl(publicUrl);

    return true;
}

void RelayService::watchSslCertificateFileIfNeeded(const conf::Settings& settings)
{
	if (settings.https().certificatePath.empty())
		return;

	NX_INFO(this, "Ssl certificate file specified: %1, watching for changes",
		settings.https().certificatePath);

	m_fileWatcher = std::make_unique<nx::utils::file_system::FileWatcher>(
		settings.https().certificateMonitorTimeout);

	const auto systemError = m_fileWatcher->subscribe(
		settings.https().certificatePath,
		[this, certificatePath = settings.https().certificatePath](
		    const auto& filePath,
		    auto systemError,
		    auto /*watchEvent*/)
		{
			if (systemError && filePath == certificatePath)
			{
				NX_WARNING(this, "SystemError %1 occured while watching ssl certificate file %2.",
					SystemError::toString(systemError), filePath);
				return;
			}

			NX_INFO(this, "Ssl certificate file %1 changed, restarting service", filePath);
			restart();
		},
		&m_subscriptionId);

	if (systemError)
	{
		NX_WARNING(this, "Failed to watch ssl certificate file for changes: %1",
			SystemError::toString(systemError));
	}
}

} // namespace relay
} // namespace cloud
} // namespace nx