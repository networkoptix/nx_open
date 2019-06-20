#include <vector>

#include <nx/network/socket_common.h>
#include <nx/utils/service.h>

namespace nx::cloud::storage::service {

class StorageService:
    public nx::utils::Service
{
    using base_type = nx::utils::Service;

public:
    StorageService(int argc, char** argv);

    std::vector<network::SocketAddress> httpEndpoints() const;
    std::vector<network::SocketAddress> httpsEndpoints() const;

protected:
    virtual std::unique_ptr<utils::AbstractServiceSettings> createSettings() override;
    virtual int serviceMain(const utils::AbstractServiceSettings& settings) override;
};

} // namespace nx::cloud::storage::service
