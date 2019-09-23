#include <gtest/gtest.h>

#include <nx/network/ssl/ssl_engine.h>
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

        connection()->setVerifyCertificateCallback(
            [this](auto&&... args)
            {
                return verifyServerCertificate(std::forward<decltype(args)>(args)...);
            });

        whenSynchronouslyConnectToServer();

        assertTheActualCertificateIsUsed();
    }

    void assertTheActualCertificateIsUsed()
    {
        ASSERT_EQ(m_serialNumber, m_lastVerifiedCertificateSerialNumber);
    }

private:
    nx::String m_certificate;
    long m_serialNumber = 0;
    long m_lastVerifiedCertificateSerialNumber = 0;

    void installRandomCertificate()
    {
        m_certificate = ssl::Engine::makeCertificateAndKey(
            "nx_network_ut", "US", "Network Optix", ++m_serialNumber);

        ssl::Engine::useCertificateAndPkey(m_certificate);
    }

    bool verifyServerCertificate(const Certificate& certificate)
    {
        m_lastVerifiedCertificateSerialNumber = certificate.serialNumber;

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
