#pragma once

#include <set>

#include <nx/cloud/cdb/managers/system_health_info_provider.h>

namespace nx {
namespace cdb {
namespace test {

class SystemHealthInfoProviderStub:
    public AbstractSystemHealthInfoProvider
{
public:
    virtual bool isSystemOnline(const std::string& systemId) const override;

    virtual void getSystemHealthHistory(
        const AuthorizationInfo& authzInfo,
        data::SystemId systemId,
        std::function<void(api::ResultCode, api::SystemHealthHistory)> completionHandler) override;

    void setSystemStatus(const std::string& id, bool online);

private:
    std::set<std::string> m_onlineSystems;
};

} // namespace test
} // namespace cdb
} // namespace nx
