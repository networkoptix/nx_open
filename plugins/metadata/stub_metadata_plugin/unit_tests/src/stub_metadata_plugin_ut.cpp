#include <iostream>

#include <nx/kit/test.h>

#include <plugins/plugin_api.h>

extern "C" {

nxpl::PluginInterface* createNxMetadataPlugin();

} // extern "C"

TEST(stub_metadata_plugin, test)
{
    nxpl::PluginInterface* plugin = createNxMetadataPlugin();

    // TODO: #mshevchenko: Test some features.
    ASSERT_TRUE(true);

    plugin->releaseRef();
}

int main()
{
    return nx::kit::test::runAllTests();
}
