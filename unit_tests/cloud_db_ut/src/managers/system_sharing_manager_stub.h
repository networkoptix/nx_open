#pragma once

#include <managers/system_sharing_manager.h>

namespace nx {
namespace cdb {
namespace test {

class SystemSharingManagerStub:
    public AbstractSystemSharingManager
{
public:
    virtual api::SystemAccessRole getAccountRightsForSystem(
        const std::string& accountEmail,
        const std::string& systemId) const override;
    
    virtual boost::optional<api::SystemSharingEx> getSystemSharingData(
        const std::string& accountEmail,
        const std::string& systemId) const override;

    virtual void addSystemSharingExtension(AbstractSystemSharingExtension* extension) override;
    virtual void removeSystemSharingExtension(AbstractSystemSharingExtension* extension) override;
};

} // namespace test
} // namespace cdb
} // namespace nx
