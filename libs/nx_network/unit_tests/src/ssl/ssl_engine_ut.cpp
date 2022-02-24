// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/ssl/context.h>
#include <nx/network/test_support/stream_socket_acceptance_tests.h>

#include "ssl_socket_ut.h"

namespace nx::network::ssl::test {

class SslEngine:
    public network::test::StreamSocketAcceptance<SslSocketBothEndsEncryptedTypeSet>
{
    using base_type = network::test::StreamSocketAcceptance<SslSocketBothEndsEncryptedTypeSet>;

protected:
    virtual void SetUp() override
    {
        installRandomCertificate();

        base_type::SetUp();
    }

    void givenWorkingSslServer()
    {
        givenSynchronousPingPongServer();
    }

    void whenReplaceCertificate()
    {
        installRandomCertificate();
    }

    void whenReinstantiateSslServer()
    {
        givenSynchronousPingPongServer();
    }

    void thenSslServerUsesTheNewCertificate()
    {
        givenClientSocket();

        connection()->setSyncVerifyCertificateChainCallback(
            [this](auto&&... args)
            {
                return verifyServerCertificate(std::forward<decltype(args)>(args)...);
            });

        whenSynchronouslyConnectToServer();

        assertTheActualCertificateIsUsed();
    }

    void assertTheActualCertificateIsUsed()
    {
        ASSERT_EQ(m_serialNumber, m_lastVerifiedCertificate.serialNumber());
    }

private:
    std::string m_certificate;
    long m_serialNumber = 0;
    Certificate m_lastVerifiedCertificate;

    void installRandomCertificate()
    {
        m_certificate = ssl::makeCertificateAndKey(
            {"nx_network_ut", "US", "Organization"}, "localhost", ++m_serialNumber);

        ssl::Context::instance()->setDefaultCertificate(m_certificate);
    }

    bool verifyServerCertificate(
        nx::network::ssl::CertificateChainView chain,
        nx::network::ssl::StreamSocket* /*socket*/)
    {
        m_lastVerifiedCertificate = nx::network::ssl::Certificate(chain.front());
        return true;
    }
};

//-------------------------------------------------------------------------------------------------

TEST_F(SslEngine, replace_certificate_is_used_by_server_socket)
{
    givenWorkingSslServer();

    whenReplaceCertificate();
    whenReinstantiateSslServer();

    thenSslServerUsesTheNewCertificate();
}

} // namespace nx::network::ssl::test
