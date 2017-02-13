#include "generate_data.h"

#include "load_emulator.h"

namespace nx {
namespace cdb {
namespace client {

int generateSystems(
    const std::string& cdbUrl,
    const std::string& login,
    const std::string& password,
    int testSystemsToGenerate)
{
    auto loadEmulator = std::make_unique<test::LoadEmulator>(
        cdbUrl,
        login,
        password);

    loadEmulator->createRandomSystems(testSystemsToGenerate);
    return 0;
}

} // namespace client
} // namespace cdb
} // namespace nx
