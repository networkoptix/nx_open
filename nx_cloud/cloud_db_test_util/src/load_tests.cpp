#include "load_tests.h"

#include <chrono>
#include <thread>

#include "load_emulator.h"

namespace nx {
namespace cdb {
namespace client {

int establishManyConnections(
    const std::string& cdbUrl,
    const std::string& login,
    const std::string& password)
{
    auto loadEmulator = std::make_unique<test::LoadEmulator>(
        cdbUrl, //"https://cloud-dev.hdw.mx",
        login,
        password);

    loadEmulator->setTransactionConnectionCount(1000);
    loadEmulator->start();

    std::this_thread::sleep_for(std::chrono::hours(1));

    return 0;
}

} // namespace client
} // namespace cdb
} // namespace nx
