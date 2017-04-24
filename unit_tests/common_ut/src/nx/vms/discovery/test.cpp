#include <gtest/gtest.h>

#include <nx/vms/discovery/udp_multicast_finder.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace vms {
namespace discovery {
namespace test {

class UdpMulticastModuleFinder:
    public testing::Test
{
public:
    virtual ~UdpMulticastModuleFinder() override
    {
        moduleFinder.pleaseStopSync();
    }

protected:
    discovery::UdpMulticastFinder moduleFinder;
};

// Only makes sense to use across real network.
TEST_F(UdpMulticastModuleFinder, DISABLED_RealUse)
{
    moduleFinder.listen(
        [this](const QnModuleInformationWithAddresses& module)
        {
            NX_LOGX(lm("Found module: %1").str(module.id), cl_logINFO);
        });

    QnModuleInformationWithAddresses information;
    information.type = lit("test");
    information.customization = lit("test");
    information.brand = lit("test");
    information.version = QnSoftwareVersion(1, 2, 3, 123);
    information.type = lit("test");
    information.name = lit("test");
    while (true)
    {
        information.id = QnUuid::createUuid();
        NX_LOGX(lm("New module information: %1").str(information.id), cl_logINFO);
        moduleFinder.multicastInformation(information);
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

} // namespace test
} // namespace discovery
} // namespace vms
} // namespace nx
