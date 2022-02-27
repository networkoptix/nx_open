// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ssl_socket_ut.h"

#include <thread>

#include <gtest/gtest.h>
#include <openssl/ssl.h>

#include <nx/network/aio/async_channel_reflector.h>
#include <nx/network/ssl/context.h>
#include <nx/network/test_support/simple_socket_test_helper.h>
#include <nx/network/test_support/socket_test_helper.h>
#include <nx/network/test_support/stream_socket_acceptance_tests.h>
#include <nx/utils/thread/mutex.h>

#include "sync_ssl_socket.h"

namespace nx::network {

namespace test {

using namespace nx::network::ssl::test;

INSTANTIATE_TYPED_TEST_SUITE_P(
    SslSocketBothEndsEncrypted,
    StreamSocketAcceptance,
    SslSocketBothEndsEncryptedTypeSet);

INSTANTIATE_TYPED_TEST_SUITE_P(
    SslSocketBothEndsEncryptedAutoDetectingServer,
    StreamSocketAcceptance,
    SslSocketBothEndsEncryptedAutoDetectingServerTypeSet);

INSTANTIATE_TYPED_TEST_SUITE_P(
    SslSocketClientNotEncrypted,
    StreamSocketAcceptance,
    SslSocketClientNotEncryptedTypeSet);

} // namespace test

//-------------------------------------------------------------------------------------------------

namespace ssl::test {

namespace {

class AbstractReflectorPool
{
public:
    virtual ~AbstractReflectorPool() = default;

    virtual void add(std::unique_ptr<AbstractStreamSocket> streamSocket) = 0;
    virtual std::size_t count() const = 0;
    virtual std::unique_ptr<AbstractStreamSocket> takeSocket() = 0;
};

class SynchronousReflector
{
public:
    SynchronousReflector(std::unique_ptr<AbstractStreamSocket> streamSocket):
        m_streamSocket(std::move(streamSocket)),
        m_terminated(false)
    {
    }

    ~SynchronousReflector()
    {
        stop();
    }

    void stop()
    {
        if (!m_terminated)
        {
            m_terminated = true;
            m_ioThread.join();
        }
    }

    void start()
    {
        m_ioThread = std::thread(
            std::bind(&SynchronousReflector::ioThreadMain, this));
    }

    std::unique_ptr<AbstractStreamSocket> takeSocket()
    {
        stop();

        decltype(m_streamSocket) result;
        result.swap(m_streamSocket);
        return result;
    }

private:
    std::thread m_ioThread;
    std::unique_ptr<AbstractStreamSocket> m_streamSocket;
    std::atomic<bool> m_terminated;

    void ioThreadMain()
    {
        if (!m_streamSocket->setRecvTimeout(std::chrono::milliseconds(1)) ||
            !m_streamSocket->setSendTimeout(std::chrono::milliseconds(1)))
        {
            return;
        }

        while (!m_terminated)
        {
            std::array<char, 4 * 1024> readBuf;

            int bytesRead = m_streamSocket->recv(readBuf.data(), (int)readBuf.size());
            if (bytesRead <= 0)
            {
                if (SystemError::getLastOSErrorCode() == SystemError::timedOut)
                    continue;
                break;
            }

            int bytesWritten = m_streamSocket->send(readBuf.data(), bytesRead);
            if (bytesWritten != bytesRead)
            {
                if (SystemError::getLastOSErrorCode() == SystemError::timedOut)
                    continue;
                break;
            }
        }
    }
};

//-------------------------------------------------------------------------------------------------

class SynchronousReflectorPool:
    public AbstractReflectorPool
{
public:
    virtual ~SynchronousReflectorPool()
    {
        for (auto& syncReflector: m_reflectors)
            syncReflector.reset();
    }

    virtual void add(std::unique_ptr<AbstractStreamSocket> streamSocket) override
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        ASSERT_TRUE(streamSocket->setNonBlockingMode(false));
        auto syncReflector = std::make_unique<SynchronousReflector>(std::move(streamSocket));
        syncReflector->start();
        m_reflectors.push_back(std::move(syncReflector));
    }

    virtual std::size_t count() const override
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        return m_reflectors.size();
    }

    virtual std::unique_ptr<AbstractStreamSocket> takeSocket() override
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        auto result = m_reflectors.front()->takeSocket();
        m_reflectors.pop_front();
        return result;
    }

private:
    std::deque<std::unique_ptr<SynchronousReflector>> m_reflectors;
    mutable nx::Mutex m_mutex;
};

//-------------------------------------------------------------------------------------------------

using AbstractStreamSocketAsyncReflector = aio::AsyncChannelReflector<AbstractStreamSocket>;

class AsynchronousReflectorPool:
    public AbstractReflectorPool
{
public:
    AsynchronousReflectorPool():
        m_terminated(false)
    {
    }

    virtual ~AsynchronousReflectorPool()
    {
        decltype (m_reflectors) acceptedConnections;
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            m_terminated = true;
            acceptedConnections.swap(m_reflectors);
        }

        for (auto& connection: acceptedConnections)
            connection->pleaseStopSync();
    }

    virtual void add(std::unique_ptr<AbstractStreamSocket> streamSocket) override
    {
        using namespace std::placeholders;

        ASSERT_TRUE(streamSocket->setNonBlockingMode(true));

        NX_MUTEX_LOCKER lock(&m_mutex);

        m_reflectors.push_back(nullptr);
        auto asyncReflector =
            std::make_unique<AbstractStreamSocketAsyncReflector>(
                std::move(streamSocket));
        asyncReflector->start(
            std::bind(&AsynchronousReflectorPool::onReflectorDone, this,
                --m_reflectors.end(), _1));
        m_reflectors.back() = std::move(asyncReflector);
    }

    virtual std::size_t count() const override
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_reflectors.size();
    }

    virtual std::unique_ptr<AbstractStreamSocket> takeSocket() override
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        auto result = m_reflectors.front()->takeChannel();
        m_reflectors.pop_front();
        return result;
    }

private:
    std::list<std::unique_ptr<AbstractStreamSocketAsyncReflector>> m_reflectors;
    bool m_terminated;
    mutable nx::Mutex m_mutex;

    void onReflectorDone(
        std::list<std::unique_ptr<AbstractStreamSocketAsyncReflector>>::iterator reflectorIter,
        SystemError::ErrorCode /*sysErrorCode*/)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (!m_terminated)
            m_reflectors.erase(reflectorIter);
    }
};

} // namespace

//-------------------------------------------------------------------------------------------------
// Test fixture

/**
 * These tests use SyncSslSocket on one side to guarantee that ssl is actually implied in test.
 */
class SslSocketVerifySslIsActuallyUsed:
    public ::testing::Test
{
public:
    SslSocketVerifySslIsActuallyUsed():
        m_clientSocket(std::make_unique<TCPSocket>(AF_INET)),
        m_reflectorPoolInUse(nullptr)
    {
        switchToSynchronousMode();

        startAcceptor();
    }

    ~SslSocketVerifySslIsActuallyUsed()
    {
        stopAcceptor();
    }

    void switchToSynchronousMode()
    {
        m_reflectorPoolInUse = &m_synchronousReflectorPool;
    }

    void switchToAsynchronousMode()
    {
        m_reflectorPoolInUse = &m_asynchronousReflectorPool;
    }

protected:
    void givenEstablishedConnection()
    {
        ASSERT_TRUE(m_clientSocket.connect(m_acceptor->getLocalAddress(), nx::network::kNoTimeout));
    }

    void whenSentRandomData()
    {
        prepareTestData();

        ASSERT_EQ(
            m_testData.size(),
            m_clientSocket.send(m_testData.data(), m_testData.size()));
    }

    void thenSameDataHasBeenReceivedInResponse()
    {
        nx::Buffer response;
        response.resize(m_testData.size());
        ASSERT_EQ(
            m_testData.size(),
            m_clientSocket.recv(response.data(), response.size(), MSG_WAITALL));

        ASSERT_EQ(m_testData, response);
    }

    void stopAcceptor()
    {
        if (m_acceptor)
            m_acceptor->pleaseStopSync();
    }

    AbstractReflectorPool* reflectorPoolInUse()
    {
        return m_reflectorPoolInUse;
    }

    AbstractReflectorPool* unusedReflectorPool()
    {
        return m_reflectorPoolInUse == &m_asynchronousReflectorPool
            ? static_cast<AbstractReflectorPool*>(&m_synchronousReflectorPool)
            : static_cast<AbstractReflectorPool*>(&m_asynchronousReflectorPool);
    }

private:
    std::unique_ptr<ssl::StreamServerSocket> m_acceptor;
    ClientSyncSslSocket m_clientSocket;
    nx::Buffer m_testData;
    AsynchronousReflectorPool m_asynchronousReflectorPool;
    SynchronousReflectorPool m_synchronousReflectorPool;
    AbstractReflectorPool* m_reflectorPoolInUse;

    void startAcceptor()
    {
        using namespace std::placeholders;

        m_acceptor = std::make_unique<ssl::StreamServerSocket>(
            ssl::Context::instance(),
            std::make_unique<TCPServerSocket>(AF_INET),
            EncryptionUse::autoDetectByReceivedData);

        ASSERT_TRUE(m_acceptor->setNonBlockingMode(true));
        ASSERT_TRUE(m_acceptor->bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_acceptor->listen());
        m_acceptor->acceptAsync(
            std::bind(&SslSocketVerifySslIsActuallyUsed::onAcceptCompletion, this, _1, _2));
    }

    void onAcceptCompletion(
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<AbstractStreamSocket> streamSocket)
    {
        using namespace std::placeholders;

        ASSERT_EQ(SystemError::noError, sysErrorCode);
        ASSERT_TRUE(streamSocket->setNonBlockingMode(true));

        m_acceptor->acceptAsync(
            std::bind(&SslSocketVerifySslIsActuallyUsed::onAcceptCompletion, this, _1, _2));

        m_reflectorPoolInUse->add(std::move(streamSocket));
    }

    void prepareTestData()
    {
        m_testData.resize(nx::utils::random::number<int>(1024, 4096));
        std::generate(m_testData.data(), m_testData.data() + m_testData.size(), &rand);
    }
};

// Verifies by using ClientSyncSslSocket class on one side.

TEST_F(SslSocketVerifySslIsActuallyUsed, ssl_used_by_async_io)
{
    switchToAsynchronousMode();

    givenEstablishedConnection();
    whenSentRandomData();
    thenSameDataHasBeenReceivedInResponse();
}

TEST_F(SslSocketVerifySslIsActuallyUsed, ssl_used_by_sync_io)
{
    switchToSynchronousMode();

    givenEstablishedConnection();
    whenSentRandomData();
    thenSameDataHasBeenReceivedInResponse();
}

//-------------------------------------------------------------------------------------------------

class SslSocketSpecific:
    public network::test::StreamSocketAcceptance<
        network::test::SslSocketBothEndsEncryptedAutoDetectingServerTypeSet>
{
    using base_type = network::test::StreamSocketAcceptance<
        network::test::SslSocketBothEndsEncryptedAutoDetectingServerTypeSet>;

public:
    ~SslSocketSpecific()
    {
        if (m_silentServer)
            m_silentServer->pleaseStopSync();

        m_acceptedConnections.clear();
    }

protected:
    void givenSocketTimedOutOnSendAsync()
    {
        givenAcceptingServerSocket();
        givenConnectedSocket();
        setClientSocketSendTimeout(std::chrono::milliseconds(1));

        whenClientSendsRandomDataAsyncNonStop();

        thenClientSendTimesOutEventually();
    }

    void givenSocketTimedOutOnSend()
    {
        givenAcceptingServerSocket();
        givenConnectedSocket();
        setClientSocketSendTimeout(std::chrono::milliseconds(1));

        const auto randomData = nx::utils::generateRandomName(64*1024);
        for (;;)
        {
            int bytesSent =
                connection()->send(randomData.data(), randomData.size());
            if (bytesSent >= 0)
            {
                m_dataSent += randomData;
                continue;
            }

            ASSERT_EQ(SystemError::timedOut, SystemError::getLastOSErrorCode());
            break;
        }
    }

    void givenNotEncryptedConnection()
    {
        m_notEncryptedConnection = std::make_unique<TCPSocket>(AF_INET);
        ASSERT_TRUE(m_notEncryptedConnection->connect(serverEndpoint(), kNoTimeout))
            << SystemError::getLastOSErrorText();

        nx::Buffer testData("Hello, world");
        ASSERT_EQ(
            testData.size(),
            m_notEncryptedConnection->send(testData.data(), testData.size()));
    }

    void givenSilentServer()
    {
        m_silentServer = std::make_unique<TCPServerSocket>(AF_INET);

        ASSERT_TRUE(m_silentServer->bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_silentServer->listen());
        ASSERT_TRUE(m_silentServer->setNonBlockingMode(true));

        startAccepting();
    }

    void whenConnectSyncWithTimeout(std::chrono::milliseconds timeout)
    {
        m_prevConnectResult = connection()->connect(m_silentServer->getLocalAddress(), timeout)
            ? SystemError::noError
            : SystemError::getLastOSErrorCode();
    }

    void whenConnectAsyncWithTimeout(std::chrono::milliseconds timeout)
    {
        ASSERT_TRUE(connection()->setNonBlockingMode(true));
        ASSERT_TRUE(connection()->setSendTimeout(timeout.count()));

        std::promise<void> done;
        connection()->connectAsync(
            m_silentServer->getLocalAddress(),
            [this, &done](SystemError::ErrorCode errorCode)
            {
                m_prevConnectResult = errorCode;
                done.set_value();
            });

        done.get_future().wait();
    }

    void thenConnectFailed(SystemError::ErrorCode expected)
    {
        ASSERT_EQ(expected, m_prevConnectResult);
    }

    void assertServerReceivedAllDataSent()
    {
        assertServerReceivedData(m_dataSent);
    }

    void assertServerReceivedData(const nx::Buffer& expected)
    {
        nx::Buffer dataReceived;
        dataReceived.reserve(expected.size());
        while (dataReceived.size() < expected.size())
        {
            const int bytesRead = lastAcceptedSocket()->recv(
                dataReceived.data() + dataReceived.size(),
                dataReceived.capacity() - dataReceived.size());
            ASSERT_GT(bytesRead, 0);
            dataReceived.resize(dataReceived.size() + bytesRead);
        }

        ASSERT_EQ(expected, dataReceived);
    }

    void assertServerReceivedPing()
    {
        assertServerReceivedData(clientMessage());
    }

    void assertClientConnectionReportsEncryptionEnabled()
    {
        ASSERT_TRUE(connection()->isEncryptionEnabled());
    }

    void assertServerConnectionReportsEncryptionEnabled()
    {
        thenConnectionHasBeenAccepted();

        ASSERT_TRUE(
            static_cast<AbstractEncryptedStreamSocket*>(lastAcceptedSocket())
                ->isEncryptionEnabled());
    }

    void assertServerConnectionReportsEncryptionDisabled()
    {
        thenConnectionHasBeenAccepted();

        ASSERT_FALSE(
            static_cast<AbstractEncryptedStreamSocket*>(lastAcceptedSocket())
                ->isEncryptionEnabled());
    }

    void assertServerNameHasBeenPassedByTheClientConnection()
    {
        thenConnectionHasBeenAccepted();

        ASSERT_EQ(
            "127.0.0.1",
            static_cast<AbstractEncryptedStreamSocket*>(lastAcceptedSocket())->serverName());
    }

private:
    std::unique_ptr<TCPServerSocket> m_silentServer;
    std::unique_ptr<TCPSocket> m_notEncryptedConnection;
    nx::Buffer m_dataSent;
    std::optional<SystemError::ErrorCode> m_prevConnectResult;
    std::vector<std::unique_ptr<AbstractStreamSocket>> m_acceptedConnections;

    void startAccepting()
    {
        m_silentServer->acceptAsync(
            [this](
                SystemError::ErrorCode /*result*/,
                std::unique_ptr<AbstractStreamSocket> connection)
            {
                m_acceptedConnections.push_back(std::move(connection));
                startAccepting();
            });
    }
};

TEST_F(SslSocketSpecific, socket_becomes_unusable_after_async_send_timeout)
{
    givenSocketTimedOutOnSendAsync();

    whenClientSendsPingAsync();

    thenSendFailedWithUnrecoverableError();
    assertClientConnectionReportsNotConnected();
}

// TODO: #akolesnikov Probably, it is still possible to create a work-around for openssl limitation
// of retrying write only with the same arguments (NXLIB-1).
TEST_F(SslSocketSpecific, DISABLED_socket_is_still_usable_after_sync_send_timeout_error)
{
    givenSocketTimedOutOnSend();

    thenConnectionHasBeenAccepted();
    assertServerReceivedAllDataSent();

    setClientSocketSendTimeout(kNoTimeout);
    whenClientSendsPing();

    assertServerReceivedPing();
}

TEST_F(SslSocketSpecific, socket_becomes_unusable_after_sync_send_timeout)
{
    givenSocketTimedOutOnSend();

    whenClientSendsPing();

    thenSendFailedWithUnrecoverableError();
    assertClientConnectionReportsNotConnected();
}

TEST_F(SslSocketSpecific, enabled_encryption_is_reported)
{
    givenAcceptingServerSocket();
    givenConnectedSocket();

    assertClientConnectionReportsEncryptionEnabled();
    assertServerConnectionReportsEncryptionEnabled();
}

TEST_F(SslSocketSpecific, disabled_encryption_is_reported)
{
    givenAcceptingServerSocket();
    givenNotEncryptedConnection();

    assertServerConnectionReportsEncryptionDisabled();
}

TEST_F(SslSocketSpecific, servername_tls_extension_is_passed)
{
    givenAcceptingServerSocket();
    givenConnectedSocket();

    assertServerNameHasBeenPassedByTheClientConnection();
}

TEST_F(SslSocketSpecific, sync_connect_timeout_is_respected)
{
    givenSilentServer();
    givenClientSocket();

    whenConnectSyncWithTimeout(std::chrono::milliseconds(1));

    thenConnectFailed(SystemError::timedOut);
}

TEST_F(SslSocketSpecific, async_connect_timeout_is_respected)
{
    givenSilentServer();
    givenClientSocket();

    whenConnectAsyncWithTimeout(std::chrono::milliseconds(1));

    thenConnectFailed(SystemError::timedOut);
}

//-------------------------------------------------------------------------------------------------

struct BothEndsNotEncryptedTypeSet
{
    using ClientSocket = TCPSocket;
    using ServerSocket = TCPServerSocket;
};

class SslSocketSpecificHandshake:
    public network::test::StreamSocketAcceptance<BothEndsNotEncryptedTypeSet>
{
    using base_type = network::test::StreamSocketAcceptance<BothEndsNotEncryptedTypeSet>;

public:
    ~SslSocketSpecificHandshake()
    {
        if (m_encryptedConnection)
            m_encryptedConnection->pleaseStopSync();
    }

protected:
    void whenDoHandshake()
    {
        m_encryptedConnection = std::make_unique<ssl::ClientStreamSocket>(
            Context::instance(), takeConnection(), kAcceptAnyCertificateCallback);
        ASSERT_TRUE(m_encryptedConnection->setNonBlockingMode(true));
        m_encryptedConnection->handshakeAsync(
            [this](SystemError::ErrorCode errorCode)
            {
                m_handshakeResult.push(errorCode);
            });
    }

    void thenHandshakeFailedWith(SystemError::ErrorCode expected)
    {
        ASSERT_EQ(expected, m_handshakeResult.pop());
    }

private:
    nx::utils::SyncQueue<SystemError::ErrorCode> m_handshakeResult;
    std::unique_ptr<ssl::ClientStreamSocket> m_encryptedConnection;
};

TEST_F(SslSocketSpecificHandshake, handshake_time_is_limited_by_send_timeout)
{
    givenListeningServerSocket();
    givenConnectedSocket();
    setClientSocketSendTimeout(std::chrono::milliseconds(1));

    whenDoHandshake();

    thenHandshakeFailedWith(SystemError::timedOut);
}

//-------------------------------------------------------------------------------------------------

class SslSocketCertificateVerification:
    public network::test::StreamSocketAcceptance<SslSocketBothEndsEncryptedTypeSet>
{
    using base_type = network::test::StreamSocketAcceptance<SslSocketBothEndsEncryptedTypeSet>;

public:
    ~SslSocketCertificateVerification()
    {
        if (connection())
            connection()->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        const auto pem = makeCertificateAndKey({"test", "US", "Pupkin"}, "localhost", 0x485559);
        ASSERT_TRUE(Context::instance()->setDefaultCertificate(pem));

        X509Certificate x509;
        ASSERT_TRUE(x509.parsePem(pem));

        m_certificate = x509.certificates().front();

        base_type::SetUp();

        givenAcceptingServerSocket();
    }

    const Certificate& installedCertificate() const
    {
        return m_certificate;
    }

    void installApprovingCertificateVerifier()
    {
        installApprovingCertificateVerifier(connection());
    }

    void installApprovingCertificateVerifier(ssl::StreamSocket* socket)
    {
        socket->setVerifyCertificateCallback(
            [this](bool /*isPreverifyOk*/, auto x509Ctx, auto /*pipeline*/)
            {
                Certificate certificate(X509_STORE_CTX_get_current_cert(x509Ctx));
                NX_VERBOSE(this, "Received certificate (%1; %2)",
                    certificate.serialNumber(), certificate.issuer());
                m_receivedCertificates.push(std::move(certificate));
                return true;
            });
    }

    void installRejectingCertificateVerifier()
    {
        connection()->setVerifyCertificateCallback([](auto&&...) { return false; });
    }

    void thenCertificateIsReceived()
    {
        m_lastReceivedCertificate = m_receivedCertificates.pop();
    }

    void andCertificateAttributesAreExpected()
    {
        ASSERT_EQ(
            installedCertificate().issuer(),
            m_lastReceivedCertificate->issuer());

        ASSERT_EQ(installedCertificate().serialNumber(), m_lastReceivedCertificate->serialNumber());
    }

    const Certificate& lastReceivedCertificate() const
    {
        return *m_lastReceivedCertificate;
    }

private:
    Certificate m_certificate;
    nx::utils::SyncQueue<Certificate> m_receivedCertificates;
    std::optional<Certificate> m_lastReceivedCertificate;
};

TEST_F(SslSocketCertificateVerification, connect_succeeds_if_server_certificate_is_verified)
{
    givenClientSocket();
    installApprovingCertificateVerifier();

    whenConnectToServerAsync();

    thenConnectionIsEstablished();
}

TEST_F(SslSocketCertificateVerification, connect_fails_if_server_certificate_is_not_verified)
{
    givenClientSocket();
    installRejectingCertificateVerifier();

    whenConnectToServerAsync();

    thenConnectionIsNotEstablished();
}

TEST_F(SslSocketCertificateVerification, the_callback_receives_certificate_information)
{
    givenClientSocket();
    installApprovingCertificateVerifier();

    whenConnectToServerAsync();

    thenCertificateIsReceived();
    andCertificateAttributesAreExpected();
}

//-------------------------------------------------------------------------------------------------

class SslSocketCertificateSelection:
    public SslSocketCertificateVerification
{
    using base_type = SslSocketCertificateVerification;

public:
    ~SslSocketCertificateSelection()
    {
        for (auto& hostname: m_virtualHosts)
        {
            SocketGlobals::addressResolver().dnsResolver().removeEtcHost(hostname);
        }
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        setRandomCertificateAsDefault();
    }

    void replaceDefaultCertificate()
    {
        setRandomCertificateAsDefault();
    }

    void registerMultipleCertificatesOnDifferentVirtualHosts(int count = 3)
    {
        m_virtualHosts = registerVirtualDnsNames(count);

        setRandomCertificatesForVirtualHosts();
    }

    void replaceAllVirtualHostsCertificates()
    {
        setRandomCertificatesForVirtualHosts();
    }

    void setRandomCertificatesForVirtualHost(const std::string& hostname)
    {
        const auto pem = makeCertificateAndKey({"Test", "US", "NX"}, hostname);
        ASSERT_TRUE(Context::instance()->configureVirtualHost(hostname, pem));

        X509Certificate x509;
        ASSERT_TRUE(x509.parsePem(pem));

        m_hostToCertificate[hostname] = x509.certificates().front();
    }

    void whenConnectToRandomVirtualHost()
    {
        whenConnectToVirtualHost(nx::utils::random::choice(m_virtualHosts));
    }

    void whenConnectToLocalHost()
    {
        whenConnectToVirtualHost("127.0.0.1");
    }

    void whenConnectToVirtualHost(const std::string& targetHostname)
    {
        m_lastConnectionHostname = targetHostname;

        givenClientSocket();
        installApprovingCertificateVerifier();

        whenConnectToServerAsync(
            SocketAddress(m_lastConnectionHostname, serverEndpoint().port),
            []() {});
    }

    void whenRemoveVirtualHost(const std::string& hostname)
    {
        Context::instance()->removeVirtualHost(hostname);

        m_hostToCertificate.erase(hostname);
    }

    std::string chooseRandomVirtualHostname()
    {
        return nx::utils::random::choice(m_virtualHosts);
    }

    void startConcurrentConnections(
        const std::string& hostname,
        int concurrentConnectionCount)
    {
        m_lastConnectionHostname = hostname;

        m_connectionsGenerator = std::make_unique<network::test::ConnectionsGenerator>(
            SocketAddress(hostname, serverEndpoint().port),
            concurrentConnectionCount,
            network::test::TestTrafficLimitType::outgoing,
            1,
            0,
            network::test::TestTransmissionMode::spam);

        m_connectionsGenerator->setSocketFactory(
            [this]()
            {
                auto socket = std::make_unique<ssl::StreamSocket>(
                    Context::instance(),
                    std::make_unique<TCPSocket>(AF_INET),
                    /*isServerSide*/ false,
                    kAcceptAnyCertificateCallback);
                installApprovingCertificateVerifier(socket.get());
                return socket;
            });

        m_connectionsGenerator->start();
    }

    void waitUntilCorrespondingCertificateIsReceivedFromServer(
        const std::string& hostname)
    {
        const auto it = m_hostToCertificate.find(hostname);
        const auto expected = it != m_hostToCertificate.end() ? it->second : m_defaultCertificate;
        for (;;)
        {
            thenCertificateIsReceived();
            if (lastReceivedCertificate() == expected)
                return;
        }
    }

    void thenServerChoosesProperCertificate()
    {
        thenCertificateIsReceived();

        const auto it = m_hostToCertificate.find(m_lastConnectionHostname);

        ASSERT_EQ(
            it != m_hostToCertificate.end() ? it->second : m_defaultCertificate,
            lastReceivedCertificate());
    }

    void thenServerChoosesDefaultCertificate()
    {
        thenServerChoosesProperCertificate();
    }

private:
    void setRandomCertificateAsDefault()
    {
        const auto pem = makeCertificateAndKey({"Test", "US", "NX"});
        ASSERT_TRUE(Context::instance()->setDefaultCertificate(pem));

        X509Certificate x509;
        ASSERT_TRUE(x509.parsePem(pem));
        m_defaultCertificate = x509.certificates().front();

        NX_VERBOSE(this, "Set default certificate to (%1; %2)",
            m_defaultCertificate.serialNumber(), m_defaultCertificate.issuer());
    }

    std::vector<std::string> registerVirtualDnsNames(int count)
    {
        std::vector<std::string> hostnames;
        hostnames.resize(count);
        for (auto& hostname: hostnames)
        {
            hostname = nx::utils::random::generateName(11);

            SocketGlobals::addressResolver().dnsResolver().addEtcHost(
                hostname, {HostAddress::localhost});
        }

        return hostnames;
    }

    void setRandomCertificatesForVirtualHosts()
    {
        for (const auto& hostname: m_virtualHosts)
            setRandomCertificatesForVirtualHost(hostname);
    }

private:
    std::vector<std::string> m_virtualHosts;
    std::map<std::string, Certificate> m_hostToCertificate;
    Certificate m_defaultCertificate;
    std::string m_lastConnectionHostname;
    std::unique_ptr<network::test::ConnectionsGenerator> m_connectionsGenerator;
};

TEST_F(SslSocketCertificateSelection, server_chooses_certificate_based_on_sni)
{
    registerMultipleCertificatesOnDifferentVirtualHosts();
    whenConnectToRandomVirtualHost();
    thenServerChoosesProperCertificate();
}

TEST_F(SslSocketCertificateSelection, default_certificate_is_chosen_if_no_virtual_host_matched)
{
    registerMultipleCertificatesOnDifferentVirtualHosts();

    whenConnectToLocalHost();
    thenServerChoosesDefaultCertificate();
}

TEST_F(SslSocketCertificateSelection, replacing_default_certificate)
{
    replaceDefaultCertificate();

    whenConnectToLocalHost();
    thenServerChoosesDefaultCertificate();
}

TEST_F(SslSocketCertificateSelection, replacing_virtual_host_certificate)
{
    registerMultipleCertificatesOnDifferentVirtualHosts();
    replaceAllVirtualHostsCertificates();

    whenConnectToRandomVirtualHost();
    thenServerChoosesProperCertificate();
}

TEST_F(SslSocketCertificateSelection, removing_virtual_host)
{
    registerMultipleCertificatesOnDifferentVirtualHosts();
    const auto hostname = chooseRandomVirtualHostname();

    whenRemoveVirtualHost(hostname);
    whenConnectToVirtualHost(hostname);

    thenServerChoosesDefaultCertificate();
}

TEST_F(SslSocketCertificateSelection, replacing_virtual_host_certificate_under_load)
{
    registerMultipleCertificatesOnDifferentVirtualHosts();
    const auto hostname = chooseRandomVirtualHostname();

    startConcurrentConnections(hostname, 7);
    waitUntilCorrespondingCertificateIsReceivedFromServer(hostname);

    setRandomCertificatesForVirtualHost(hostname);

    waitUntilCorrespondingCertificateIsReceivedFromServer(hostname);
}

TEST_F(SslSocketCertificateSelection, replacing_default_certificate_under_load)
{
    const auto hostname = "127.0.0.1";

    startConcurrentConnections(hostname, 7);
    waitUntilCorrespondingCertificateIsReceivedFromServer(hostname);

    replaceDefaultCertificate();

    waitUntilCorrespondingCertificateIsReceivedFromServer(hostname);
}

TEST_F(
    SslSocketCertificateSelection,
    received_default_context_is_valid_after_certificate_replacement)
{
    const auto defaultSslContext = Context::instance()->defaultServerContext();
    replaceDefaultCertificate();

    // Checking if the context is still valid.
    ASSERT_EQ(1, SSL_CTX_up_ref(defaultSslContext.get()));
    SSL_CTX_free(defaultSslContext.get());
}

//-------------------------------------------------------------------------------------------------

class SslSocketCustomSslContext:
    public ::testing::Test
{
public:
    ~SslSocketCustomSslContext()
    {
        for (auto& ctx: m_servers)
            ctx.socket->pleaseStopSync();
    }

protected:
    void givenSslServers(int count)
    {
        for (int i = 0; i < count; ++i)
            addSslServer(/*add certificate*/ true);
    }

    void givenSslServerWithoutCertificate()
    {
        addSslServer(/*no certificate*/ false);
    }

    void assertEachServerReportsItsOwnCertificate()
    {
        for (const auto& ctx: m_servers)
        {
            ClientStreamSocket client(
                m_servers.front().context.get(),
                std::make_unique<TCPSocket>(AF_INET),
                kAcceptAnyCertificateCallback);

            nx::utils::SyncQueue<Certificate> received;
            client.setVerifyCertificateCallback(
                [&received](bool /*isPreverifyOk*/, auto x509Ctx, auto /*pipeline*/)
                {
                    received.push(X509_STORE_CTX_get_current_cert(x509Ctx));
                    return true;
                });
            ASSERT_TRUE(client.connect(ctx.socket->getLocalAddress(), kNoTimeout));

            ASSERT_EQ(ctx.certificate.serialNumber(), received.pop().serialNumber());
        }
    }

    void assertAsyncConnectFails()
    {
        ClientStreamSocket client(
            m_servers.front().context.get(),
            std::make_unique<TCPSocket>(AF_INET),
            kAcceptAnyCertificateCallback);
        ASSERT_TRUE(client.setNonBlockingMode(true));

        std::promise<SystemError::ErrorCode> done;
        client.connectAsync(
            m_servers.front().socket->getLocalAddress(),
            [&done](SystemError::ErrorCode errorCode)
            {
                done.set_value(errorCode);
            });
        const auto code = done.get_future().get();

        ASSERT_EQ(SystemError::sslHandshakeError, code);
        client.pleaseStopSync();
    }

    void assertSyncConnectFails()
    {
        ClientStreamSocket client(
            m_servers.front().context.get(),
            std::make_unique<TCPSocket>(AF_INET),
            kAcceptAnyCertificateCallback);
        ASSERT_FALSE(client.connect(m_servers.front().socket->getLocalAddress(), kNoTimeout));
    }

private:
    void addSslServer(bool generateCertificate)
    {
        m_servers.resize(m_servers.size() + 1);
        auto& ctx = m_servers.back();

        ctx.context = std::make_unique<Context>();
        auto tcpSocket = std::make_unique<TCPServerSocket>(AF_INET);
        ctx.socket = std::make_unique<ssl::StreamServerSocket>(
            ctx.context.get(),
            std::move(tcpSocket),
            EncryptionUse::always);

        if (generateCertificate)
        {
            const auto pem = makeCertificateAndKey({"test", "US", "NX"});
            ASSERT_TRUE(ctx.context->setDefaultCertificate(pem));

            X509Certificate x509;
            ASSERT_TRUE(x509.parsePem(pem));

            ctx.certificate = x509.certificates().front();
        }

        ASSERT_TRUE(ctx.socket->bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(ctx.socket->listen());
        ASSERT_TRUE(ctx.socket->setNonBlockingMode(true));
        ctx.socket->acceptAsync([](auto&&...) {});
    }

private:
    struct ServerContext
    {
        Certificate certificate;
        std::unique_ptr<Context> context;
        std::unique_ptr<ssl::StreamServerSocket> socket;
    };

    std::vector<ServerContext> m_servers;
};

TEST_F(SslSocketCustomSslContext, different_servers_may_have_different_certificates)
{
    givenSslServers(2);
    assertEachServerReportsItsOwnCertificate();
}

TEST_F(SslSocketCustomSslContext, connect_to_server_without_certificate_fails)
{
    givenSslServerWithoutCertificate();

    assertAsyncConnectFails();
    assertSyncConnectFails();
}

//-------------------------------------------------------------------------------------------------

class SslSocketProtocolSet:
    public SslSocketCertificateSelection
{
protected:
    void setServerProtocolList(const std::string& protocols)
    {
        Context::instance()->setAllowedServerVersions(protocols);
    }

    void assertConnectionCanBeEstablishedUsingProtocol(int protocol)
    {
        ASSERT_TRUE(checkConnectionUsingProtocol(protocol));
    }

    void assertConnectionCannotBeEstablishedUsingProtocol(int protocol)
    {
        ASSERT_FALSE(checkConnectionUsingProtocol(protocol));
    }

private:
    bool checkConnectionUsingProtocol(int protocol)
    {
        auto tcpSocket = std::make_unique<TCPSocket>(AF_INET);
        if (!tcpSocket->connect(serverEndpoint(), kNoTimeout))
            return false;

        ClientSyncSslSocket sslSocket(std::move(tcpSocket));
        SSL_set_min_proto_version(sslSocket.ssl(), protocol);
        SSL_set_max_proto_version(sslSocket.ssl(), protocol + 1);

        return sslSocket.performHandshake();
    }
};

TEST_F(SslSocketProtocolSet, protocol_list_can_be_modified_in_runtime)
{
    // Server is already listening.

    setServerProtocolList("tls1_1|tls1_2");
    assertConnectionCanBeEstablishedUsingProtocol(TLS1_VERSION);

    setServerProtocolList("tls1_2");
    assertConnectionCannotBeEstablishedUsingProtocol(TLS1_VERSION);
}

//-------------------------------------------------------------------------------------------------
// Mixing sync & async mode.

class SslSocketSwitchIoMode:
    public SslSocketVerifySslIsActuallyUsed
{
protected:
    void exchangeDataInSyncMode()
    {
        switchToSynchronousMode();
        whenSentRandomData();
        thenSameDataHasBeenReceivedInResponse();
    }

    void changeServerSocketIoMode()
    {
        ASSERT_EQ(1U, reflectorPoolInUse()->count());
        auto socket = reflectorPoolInUse()->takeSocket();
        unusedReflectorPool()->add(std::move(socket));
    }

    void exchangeDataInAsyncMode()
    {
        switchToAsynchronousMode();
        whenSentRandomData();
        thenSameDataHasBeenReceivedInResponse();
    }
};

TEST_F(SslSocketSwitchIoMode, from_async_to_sync)
{
    switchToAsynchronousMode();

    givenEstablishedConnection();

    exchangeDataInAsyncMode();
    changeServerSocketIoMode();
    exchangeDataInSyncMode();
}

TEST_F(SslSocketSwitchIoMode, from_sync_to_async)
{
    switchToSynchronousMode();

    givenEstablishedConnection();

    exchangeDataInSyncMode();
    changeServerSocketIoMode();
    exchangeDataInAsyncMode();
}

//-------------------------------------------------------------------------------------------------
// Common socket tests. These are not enough since they do not even check ssl is actually used.

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, SslSocketEncryptedConnection,
    []()
    {
        return std::make_unique<ssl::StreamServerSocket>(
            Context::instance(),
            std::make_unique<TCPServerSocket>(AF_INET),
            EncryptionUse::always);
    },
    []()
    {
        return std::make_unique<ssl::ClientStreamSocket>(
            Context::instance(),
            std::make_unique<TCPSocket>(AF_INET),
            kAcceptAnyCertificateCallback);
    });

NX_NETWORK_BOTH_SOCKET_TEST_CASE(
    TEST, SslSocketEncryptedConnectionAutoDetected,
    []()
    {
        return std::make_unique<ssl::StreamServerSocket>(
            Context::instance(),
            std::make_unique<TCPServerSocket>(AF_INET),
            EncryptionUse::autoDetectByReceivedData);
    },
    []()
    {
        return std::make_unique<ssl::ClientStreamSocket>(
            Context::instance(),
            std::make_unique<TCPSocket>(AF_INET),
            kAcceptAnyCertificateCallback);
    });

} // namespace ssl::test
} // namespace nx::network
