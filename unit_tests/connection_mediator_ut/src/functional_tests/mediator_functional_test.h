/**********************************************************
* Dec 21, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CONNECTION_MEDIATOR_UT_TEST_SETUP_H
#define NX_CONNECTION_MEDIATOR_UT_TEST_SETUP_H

#include <future>
#include <vector>

#include <gtest/gtest.h>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/network/socket_common.h>

#include <cloud_data_provider.h>
#include <mediator_process_public.h>

#include "local_cloud_data_provider.h"
#include "mediator_connector.h"


namespace nx {
namespace hpm {

class MediatorFunctionalTest
:
    public ::testing::Test
{
public:
    //!Calls \a start
    MediatorFunctionalTest();
    ~MediatorFunctionalTest();

    void start();
    void startAndWaitUntilStarted();
    void waitUntilStarted();
    void stop();
    //!restarts process
    void restart();

    void addArg(const char* arg);

    SocketAddress endpoint() const;

    std::shared_ptr<nx::cc::MediatorClientConnection> clientConnection();
    std::shared_ptr<nx::cc::MediatorSystemConnection> systemConnection();

    void registerCloudDataProvider(AbstractCloudDataProvider* cloudDataProvider);

    AbstractCloudDataProvider::System addRandomSystem();

private:
    QString m_tmpDir;
    int m_port;
    std::vector<char*> m_args;
    std::unique_ptr<MediatorProcessPublic> m_mediatorInstance;
    std::future<int> m_mediatorProcessFuture;
    //MediatorConnector m_mediatorConnector;
    LocalCloudDataProvider m_cloudDataProvider;
};

}   //hpm
}   //nx

#endif  //NX_CONNECTION_MEDIATOR_UT_TEST_SETUP_H
