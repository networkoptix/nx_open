/**********************************************************
* Dec 21, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MEDIATOR_FUNCTIONAL_TEST_H
#define NX_MEDIATOR_FUNCTIONAL_TEST_H

#include <future>
#include <vector>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/socket_common.h>

#include "../cloud_data_provider.h"
#include "../mediator_process_public.h"

#include "local_cloud_data_provider.h"
#include "mediaserver_emulator.h"


namespace nx {
namespace hpm {

class MediatorFunctionalTest
{
public:
    //!Calls \a start
    MediatorFunctionalTest();
    ~MediatorFunctionalTest();

    void start();
    bool startAndWaitUntilStarted();
    bool waitUntilStarted();
    void stop();
    //!restarts process
    void restart();

    void addArg(const char* arg);

    SocketAddress endpoint() const;

    std::shared_ptr<nx::hpm::api::MediatorClientTcpConnection> clientConnection();
    std::shared_ptr<nx::hpm::api::MediatorServerTcpConnection> systemConnection();

    void registerCloudDataProvider(AbstractCloudDataProvider* cloudDataProvider);

    AbstractCloudDataProvider::System addRandomSystem();
    std::unique_ptr<MediaServerEmulator> addServer(
        const AbstractCloudDataProvider::System& system,
        nx::String name);
    std::unique_ptr<MediaServerEmulator> addRandomServer(
        const AbstractCloudDataProvider::System& system);
    std::unique_ptr<MediaServerEmulator> addRandomServerNotRegisteredOnMediator(
        const AbstractCloudDataProvider::System& system);
    std::vector<std::unique_ptr<MediaServerEmulator>> addRandomServers(
        const AbstractCloudDataProvider::System& system,
        size_t count);

private:
    QString m_tmpDir;
    int m_port;
    std::vector<char*> m_args;
    std::unique_ptr<MediatorProcessPublic> m_mediatorInstance;
    std::future<int> m_mediatorProcessFuture;
    std::promise<bool /*result*/> m_mediatorStartedPromise;
    //MediatorConnector m_mediatorConnector;
    LocalCloudDataProvider m_cloudDataProvider;
};

}   //hpm
}   //nx

#endif  //NX_MEDIATOR_FUNCTIONAL_TEST_H
