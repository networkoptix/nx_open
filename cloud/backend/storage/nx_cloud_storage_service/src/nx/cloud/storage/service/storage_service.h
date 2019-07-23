#include <nx/utils/service.h>

#include <nx/network/socket_common.h>

namespace nx::cloud::storage::service {

namespace conf { class Settings; }

class Controller;
class Model;
class View;

class StorageService:
    public nx::utils::Service
{
    using base_type = nx::utils::Service;

public:
    StorageService(int argc, char** argv);

    const conf::Settings& settings() const;

    std::vector<network::SocketAddress> httpEndpoints() const;
    std::vector<network::SocketAddress> httpsEndpoints() const;

protected:
    virtual std::unique_ptr<utils::AbstractServiceSettings> createSettings() override;
    virtual int serviceMain(const utils::AbstractServiceSettings& settings) override;

private:
    const conf::Settings* m_settings = nullptr;
    Model* m_model = nullptr;
    Controller* m_controller = nullptr;
    View* m_view = nullptr;
};

} // namespace nx::cloud::storage::service
