#include "mediator_scalability_test_fixture.h"

namespace nx::hpm::test {

class HttpRedirection: public MediatorScalabilityTestFixture
{
protected:
    void whenTryToConnectToServerOnDifferentMediator(int mediatorIndex = 0, bool useHttps = false)
    {
        // m_mediaServer is connected to m_mediatorCluster.mediator(0);
        auto& differentMediator = m_mediatorCluster.mediator(mediatorIndex);

        m_httpClient = std::make_unique<api::Client>(buildUrl(differentMediator, useHttps));

        api::ConnectRequest connectRequest;
        connectRequest.destinationHostName = m_mediaServer->fullName();
        connectRequest.originatingPeerId = nx::utils::generateRandomName(7);
        connectRequest.connectSessionId = nx::utils::generateRandomName(7);
        connectRequest.connectionMethods = api::ConnectionMethod::proxy;

        m_httpClient->initiateConnection(
            connectRequest,
            [this](api::ResultCode resultCode, api::ConnectResponse response)
        {
            m_connectResponseQueue.push(
                std::make_tuple(resultCode, std::move(response)));
        });
    }

    void whenTryToConnectToServerOnDifferentMediatorWithHttps(int mediatorIndex = 0)
    {
        whenTryToConnectToServerOnDifferentMediator(mediatorIndex, true);
    }

    void thenRequestIsRedirected()
    {
        auto resultCodeAndResponse = m_connectResponseQueue.pop();
        while (std::get<api::ResultCode>(resultCodeAndResponse) != api::ResultCode::ok)
            resultCodeAndResponse = m_connectResponseQueue.pop();
    }

    void andConnectionToDifferentMediatorIsEstablished()
    {
        //TODO
    }

private:
    nx::utils::Url buildUrl(const Mediator& mediator, bool useHttps) const
    {
        return network::url::Builder()
            .setScheme(useHttps
                ? network::http::kSecureUrlSchemeName
                : network::http::kUrlSchemeName)
            .setEndpoint(useHttps
                ? mediator.httpsEndpoint()
                : mediator.httpEndpoint())
            .setPath(api::kMediatorApiPrefix).toUrl();
    }

private:
    std::unique_ptr<api::Client> m_httpClient;
};

TEST_F(HttpRedirection, http_connect_request_redirected_to_correct_mediator)
{
    givenSynchronizedClusterWithServer();
    givenServerIsListeningOnMediator(0);

    whenTryToConnectToServerOnDifferentMediator(1);

    thenRequestIsRedirected();

    andConnectionToDifferentMediatorIsEstablished();
}

TEST_F(HttpRedirection, https_connect_request_redirected_to_correct_mediator)
{
    givenSynchronizedClusterWithServer();
    givenServerIsListeningOnMediator(0);

    whenTryToConnectToServerOnDifferentMediatorWithHttps(1);

    thenRequestIsRedirected();

    andConnectionToDifferentMediatorIsEstablished();
}



} //namespace nx::hpm::test