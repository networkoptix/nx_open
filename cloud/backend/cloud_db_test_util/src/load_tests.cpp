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
    int connectionCount,
    std::chrono::milliseconds maxDelayBeforeConnect)
{
    auto loadEmulator = std::make_unique<test::LoadEmulator>(
        cdbUrl, //"https://cloud-dev.hdw.mx",
        login,
        password);

    loadEmulator->setMaxDelayBeforeConnect(maxDelayBeforeConnect);
    loadEmulator->setTransactionConnectionCount(connectionCount);
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

int makeApiRequests(
    const std::string& cdbUrl,
    const std::string& login,
    const std::string& password,
    const int connectionCount,
    std::chrono::milliseconds /*maxDelayBeforeConnect*/)
{
    using namespace std::chrono;

    std::vector<nx::network::http::AsyncHttpClientPtr> requests;
    requests.resize(connectionCount);
    QUrl url(QString::fromStdString(cdbUrl));
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

} // namespace client
} // namespace cdb
} // namespace nx
