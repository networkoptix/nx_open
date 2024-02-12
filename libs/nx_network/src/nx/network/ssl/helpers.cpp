// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "helpers.h"

#include <nx/utils/uuid.h>

#include "../socket_factory.h"
#include "../nx_network_ini.h"
#include "context.h"
#include "ssl_stream_socket.h"

namespace {

// Converts CertificateChain into internal OpenSSL structure wrapped by unique_ptr.
template<typename Chain>
auto toOpenSSL(const Chain& chain)
{
    auto result = nx::utils::wrapUnique(sk_X509_new(NULL), &sk_X509_free);

    for (const auto& cert: chain)
        sk_X509_push(result.get(), cert.x509());

    return result;
}

} // namespace

namespace nx::network::ssl {

static bool defaultVerificatation(
    const CertificateChainView& chain,
    StreamSocket* socket)
{
    const auto serverName = socket->serverName();
    const bool isServerNameUuid =
        nx::Uuid::isUuidString(serverName) && !nx::Uuid(serverName).isNull();
    const auto& effectiveServerName = isServerNameUuid ? std::string() : serverName;
    std::string errorMessage;
    if (verifyBySystemCertificates(chain, effectiveServerName, &errorMessage))
        return true;

    NX_VERBOSE(socket, errorMessage);

    return false;
}

static void setVerifyCertificateCallback(
    ClientStreamSocket* socket,
    VerifyCertificateFunc verifyCertificateFunc)
{
    socket->setSyncVerifyCertificateChainCallback(
        [verifyCertificateFunc = std::move(verifyCertificateFunc)](
            CertificateChainView chain,
            StreamSocket* socket)
        {
            if (verifyCertificateFunc)
            {
                if (verifyCertificateFunc(chain))
                    return true;
            }
            else
            {
                return defaultVerificatation(chain, socket);
            }

            return false;
        });
}

static void setVerifyCertificateAsyncCallback(
    ClientStreamSocket* socket,
    VerifyCertificateAsyncFunc verifyCertificateFunc)
{
    socket->setAsyncVerifyCertificateChainCallback(
        [verifyCertificateFunc = std::move(verifyCertificateFunc)](
            CertificateChainView chain,
            StreamSocket* socket,
            VerifyCertificateCallbackAsyncCompletionHandler completionHandler)
        {
            if (verifyCertificateFunc)
            {
                verifyCertificateFunc(chain, std::move(completionHandler));
            }
            else
            {
                completionHandler(defaultVerificatation(chain, socket));
            }
        });
}

bool verifyBySystemCertificates(
    const CertificateChainView& chain,
    const std::string& hostName,
    std::string* outErrorMessage)
{
    auto converted = toOpenSSL(chain);
    return verifyBySystemCertificates(converted.get(), hostName, outErrorMessage);
}

bool verifyBySystemCertificates(
    const CertificateChain& chain,
    const std::string& hostName,
    std::string* outErrorMessage)
{
    auto converted = toOpenSSL(chain);
    return verifyBySystemCertificates(converted.get(), hostName, outErrorMessage);
}

CertificateChain completeCertificateChain(
    const CertificateChain& chain,
    bool* ok)
{
    auto converted = toOpenSSL(chain);
    return completeCertificateChain(converted.get(), ok);
}

AdapterFunc makeAdapterFunc(
    VerifyCertificateFunc verifyCertificateFunc,
    std::optional<std::string> serverName)
{
    return
        [verifyCertificateFunc = std::move(verifyCertificateFunc),
        serverName = std::move(serverName)](auto socket)
        {
            auto secureSocket = std::make_unique<ClientStreamSocket>(
                Context::instance(), std::move(socket), kAcceptAnyCertificateCallback);
            setVerifyCertificateCallback(secureSocket.get(), std::move(verifyCertificateFunc));
            if (serverName)
                secureSocket->setServerName(*serverName);
            return secureSocket;
        };
}

NX_NETWORK_API AdapterFunc makeAsyncAdapterFunc(
    VerifyCertificateAsyncFunc verifyCertificateFunc,
    std::optional<std::string> serverName)
{
    return
        [verifyCertificateFunc = std::move(verifyCertificateFunc),
            serverName = std::move(serverName)](auto socket)
    {
        auto secureSocket = std::make_unique<ClientStreamSocket>(
            Context::instance(), std::move(socket), kAcceptAnyCertificateCallback);
        setVerifyCertificateAsyncCallback(secureSocket.get(), std::move(verifyCertificateFunc));
        if (serverName)
            secureSocket->setServerName(*serverName);
        return secureSocket;
    };
}

} // namespace nx::network::ssl
