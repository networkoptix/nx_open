#include "generate_data.h"

#include <nx/utils/std/cpp14.h>

#include "data_generator.h"

namespace nx {
namespace cdb {
namespace client {

int generateSystems(
    const std::string& cdbUrl,
    const std::string& login,
    const std::string& password,
    int testSystemsToGenerate)
{
    auto dataGenerator = std::make_unique<test::DataGenerator>(
        cdbUrl,
        login,
        password);

    dataGenerator->createRandomSystems(testSystemsToGenerate);
    return 0;
}

} // namespace client
} // namespace cdb
} // namespace nx
