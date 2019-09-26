#pragma once

#include <memory>
#include <vector>

#include <nx/network/socket_common.h>
#include <nx/utils/file_watcher.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/future.h>
#include <nx/utils/service.h>
#include <nx/utils/thread/stoppable.h>

namespace nx {
namespace cloud {

namespace relaying { class AbstractListeningPeerPool; }

namespace relay {

namespace conf { class Settings; }
namespace model { class AbstractRemoteRelayPeerPool; }

class Model;
class Controller;
class View;

class RelayService:
    public nx::utils::Service
{
    using base_type = nx::utils::Service;

public:
    RelayService(int argc, char **argv);

    std::vector<network::SocketAddress> httpEndpoints() const;
    std::vector<network::SocketAddress> httpsEndpoints() const;

    const relaying::AbstractListeningPeerPool& listeningPeerPool() const;
    const model::AbstractRemoteRelayPeerPool& remoteRelayPeerPool() const;

protected:
    virtual std::unique_ptr<utils::AbstractServiceSettings> createSettings() override;
    virtual int serviceMain(const utils::AbstractServiceSettings& settings) override;

private:
	nx::utils::SubscriptionId m_subscriptionId = nx::utils::kInvalidSubscriptionId;
	std::unique_ptr<nx::utils::file_system::FileWatcher> m_fileWatcher;

    Model* m_model = nullptr;
    Controller* m_controller = nullptr;
    View* m_view = nullptr;

    bool registerThisInstanceNameInCluster(const conf::Settings& settings);

	void watchSslCertificateFileIfNeeded(const conf::Settings& settings);
};

} // namespace relay
} // namespace cloud
} // namespace nx
