// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "helpers.h"

#include <nx/utils/uuid.h>

#include "../socket_factory.h"
#include "../nx_network_ini.h"
#include "context.h"
#include "ssl_stream_socket.h"

namespace {

// Converts internal OpenSSL structure into a more convenient form.
nx::network::ssl::CertificateChain fromOpenSSL(STACK_OF(X509)* chain)
{
    nx::network::ssl::CertificateChain result;

    const int chainSize = sk_X509_num(chain);
    for (int i = 0; i < chainSize; ++i)
        result.push_back(sk_X509_value(chain, i));

    return result;
}

// Converts CertificateChain into internal OpenSSL structure wrapped by unique_ptr.
auto toOpenSSL(const nx::network::ssl::CertificateChain& chain)
{
    auto result = nx::utils::wrapUnique(sk_X509_new(NULL), &sk_X509_free);

    for (const auto& cert: chain)
        sk_X509_push(result.get(), cert.x509());

    return result;
}

} // namespace

namespace nx::network::ssl {

static void setVerifyCertificateCallback(
    ClientStreamSocket* socket,
    VerifyCertificateFunc verifyCertificateFunc)
{
    socket->setVerifyCertificateCallback(
        [socket,
        verifyCertificateFunc = std::move(verifyCertificateFunc)](
            bool /*isPreverifyOk*/,
            X509_STORE_CTX* x509Ctx,
            const Pipeline* pipeline)
        {
            if (X509_STORE_CTX_get_error_depth(x509Ctx) != 0)
                return true;

            const auto chain = X509_STORE_CTX_get0_chain(x509Ctx);

            if (verifyCertificateFunc)
            {
                if (verifyCertificateFunc(fromOpenSSL(chain)))
                    return true;
            }
            else
            {
                const auto serverName = pipeline->serverNameFromClientHello();
                const bool isServerNameUuid =
                    QnUuid::isUuidString(serverName) && !QnUuid(serverName).isNull();
                const auto& effectiveServerName = isServerNameUuid ? std::string() : serverName;
                std::string errorMessage;
                if (verifyBySystemCertificates(chain, effectiveServerName, &errorMessage))
                    return true;

                NX_VERBOSE(socket, errorMessage);
            }

            return false;
        });
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

} // namespace nx::network::ssl
