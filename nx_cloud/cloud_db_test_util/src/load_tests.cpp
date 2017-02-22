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
    const std::string& password,
    int connectionCount)
{
    auto loadEmulator = std::make_unique<test::LoadEmulator>(
        cdbUrl, //"https://cloud-dev.hdw.mx",
        login,
        password);

    loadEmulator->setTransactionConnectionCount(connectionCount);
    loadEmulator->start();

    for (;;)
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << "Active connection count " << 
            loadEmulator->activeConnectionCount() << std::endl;
    }

    return 0;
}

} // namespace client
} // namespace cdb
} // namespace nx
