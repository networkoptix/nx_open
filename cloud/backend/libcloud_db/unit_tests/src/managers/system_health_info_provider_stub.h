#pragma once

#include <set>

#include <nx/cloud/db/managers/system_health_info_provider.h>

namespace nx::cloud::db {
namespace test {

class SystemHealthInfoProviderStub:
    public AbstractSystemHealthInfoProvider
{
public:
    virtual bool isSystemOnline(const std::string& systemId) const override;

    virtual void getSystemHealthHistory(
        const AuthorizationInfo& authzInfo,
        data::SystemId systemId,
        std::function<void(api::Result, api::SystemHealthHistory)> completionHandler) override;

    void setSystemStatus(const std::string& id, bool online);

private:
    std::set<std::string> m_onlineSystems;
};

} // namespace test
} // namespace nx::cloud::db
