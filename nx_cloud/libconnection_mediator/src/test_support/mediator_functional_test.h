/**********************************************************
* Dec 21, 2015
* akolesnikov
***********************************************************/

#ifndef NX_MEDIATOR_FUNCTIONAL_TEST_H
#define NX_MEDIATOR_FUNCTIONAL_TEST_H

#include <vector>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/socket_common.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/test_support/module_instance_launcher.h>

#include "../cloud_data_provider.h"
#include "../data/listening_peer.h"
#include "../mediator_process_public.h"

#include "local_cloud_data_provider.h"
#include "mediaserver_emulator.h"


namespace nx {
namespace hpm {

class MediatorFunctionalTest
:
    public utils::test::ModuleLauncher<MediatorProcessPublic>
{
public:
    //!Calls \a start
    MediatorFunctionalTest();
    ~MediatorFunctionalTest();

    virtual bool waitUntilStarted() override;

    SocketAddress stunEndpoint() const;
    SocketAddress httpEndpoint() const;

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

    std::tuple<nx_http::StatusCode::Value, data::ListeningPeersBySystem>
        getListeningPeers() const;

private:
    QString m_tmpDir;
    int m_stunPort;
    int m_httpPort;
    //MediatorConnector m_mediatorConnector;
    LocalCloudDataProvider m_cloudDataProvider;
};

}   // namespace hpm
}   // namespace nx

#endif  //NX_MEDIATOR_FUNCTIONAL_TEST_H
