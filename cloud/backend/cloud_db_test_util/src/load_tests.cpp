#include "load_tests.h"

#include <chrono>
#include <thread>

#include <nx/utils/timer_manager.h>

#include "load_emulator.h"

namespace nx::cloud::db::client {

int LoadTest::establishManyConnections(
    const std::string& cdbUrl,
    const std::string& login,
    const std::string& password,
    const nx::utils::ArgumentParser& args)
{
    const auto maxDelayBeforeConnect =
        nx::utils::parseTimerDuration(args.get("max-delay").value_or(QString()));
    const int connectionCount = args.get<int>("connections").value_or(100);
    const bool replaceFailedConnection =
        !static_cast<bool>(args.get("--no-reopen-connection"));

    auto loadEmulator = std::make_unique<test::LoadEmulator>(
        cdbUrl, //"https://cloud-dev.hdw.mx",
        login,
        password);

    loadEmulator->setMaxDelayBeforeConnect(maxDelayBeforeConnect);
    loadEmulator->setTransactionConnectionCount(connectionCount);
    loadEmulator->setReplaceFailedConnection(replaceFailedConnection);
    loadEmulator->start();

    for (;;)
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << "Connections: "
            "active: " << loadEmulator->activeConnectionCount() << ", "
            "failed: " << loadEmulator->totalFailedConnections() << ", "
            "connected: " << loadEmulator->connectedConnections() << ". "
            << std::endl;
    }

    return 0;
}

int LoadTest::makeApiRequests(
    const std::string& cdbUrl,
    const std::string& login,
    const std::string& password,
    const int connectionCount)
{
    using namespace std::chrono;

    std::vector<nx::network::http::AsyncHttpClientPtr> requests;
    requests.resize(connectionCount);
    nx::utils::Url url(QString::fromStdString(cdbUrl));
    url.setPath("/cdb/account/get");

    std::atomic<int> failedRequests(0);
    std::atomic<int> completedRequests(0);
    std::atomic<int> responsesToWait(connectionCount);

    QnMutex mutex;
    QnWaitCondition cond;

    auto t0 = std::chrono::steady_clock::now();

    for (auto& request: requests)
    {
        std::this_thread::sleep_for(milliseconds(1));

        request = nx::network::http::AsyncHttpClient::create();
        request->setUserName(QString::fromStdString(login));
        request->setUserPassword(QString::fromStdString(password));
        request->doGet(
            url,
            [&](nx::network::http::AsyncHttpClientPtr client)
            {
                --responsesToWait;
                if (client->failed() ||
                    client->response()->statusLine.statusCode != nx::network::http::StatusCode::ok)
                {
                    ++failedRequests;
                }
                else
                {
                    ++completedRequests;
                }
                cond.wakeAll();
            });
    }

    QnMutexLocker lock(&mutex);
    while (responsesToWait > 0 )
    {
        cond.wait(lock.mutex());
    }

    std::cout<<"Total requests done: "<< connectionCount<<". "
        "OK: "<< completedRequests.load()<<", "
        "failed "<< failedRequests.load()<<". "
        "It took "<<(duration_cast<seconds>(steady_clock::now() - t0)).count()<<" seconds"<<
        std::endl;

    return 0;
}

void LoadTest::printHelp(std::ostream& os)
{
    os << R"(
Load mode:
Opens connections to random systems with names load_test_system_{something}
-l, --load              Enable load mode. Opens multiple synchronization connections
--connections=ddd       Test connection count (100 by default)
--max-delay=ddd         Maximum delay before starting connection (milliseconds). By default, no delay
--no-reopen-connection

TODO
--api-requests          Make api requests
    )";
}

} // namespace nx::cloud::db::client
