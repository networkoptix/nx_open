// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/thread/mutex.h>
#include <rtsp/srtp_encryptor.h>

#include "webrtc_utils.h"

namespace nx::webrtc {

class AbstractDtlsDelegate;

class NX_WEBRTC_API Dtls
{
public:
    enum Status
    {
        initialized,
        handshake,
        streaming,
        error
    };
    enum class Type
    {
        client,
        server
    };
private:
    enum SslError
    {
        ok,
        continue_,
        failed
    };

public:
    // Main API.
    Dtls(const std::string& id, const nx::network::ssl::Pem& pem, Type type = Type::server):
        m_id(id), m_pem(pem), m_type(type) {}
    Status handlePacket(const uint8_t* data, int size, AbstractDtlsDelegate* ice);
    Status writePacket(const uint8_t* data, int size, AbstractDtlsDelegate* ice);
    nx::rtsp::EncryptionData encryptionData() const;
    void setFingerprint(const Fingerprint& fingerprint);

private:
    // DTLS methods.
    static int bioMethodNew(BIO *bio);
    static int bioMethodFree(BIO *bio);
    static int bioMethodWrite(BIO *bio, const char *in, int inl);
    static long bioMethodCtrl(BIO * /*bio*/, int cmd, long /*num*/, void * /*ptr*/);
    static int certificateCallback(int /*preverify_ok*/, X509_STORE_CTX *ctx);
    static void infoCallback(const SSL *ssl, int where, int ret);
    bool initDtls();
    SslError sslErrorCheck(int success, const std::string& message);
    SslError sslDumpError(const std::string& error, const std::string& message);
    std::string sslErrorString(unsigned long lastError);

private:
    mutable nx::Mutex m_mutex{nx::Mutex::RecursionMode::Recursive};
    Status m_status = Status::initialized;
    std::string m_id;
    nx::network::ssl::Pem m_pem;
    nx::rtsp::EncryptionData m_encryptionData;
    Fingerprint m_fingerprint;
    AbstractDtlsDelegate* m_ice = nullptr;
    Type m_type = Type::server;

    // DTLS variables.
    std::unique_ptr<BIO_METHOD, decltype(&BIO_meth_free)> m_bioMethods { nullptr, &BIO_meth_free };
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> m_sslContext { nullptr, &SSL_CTX_free };
    std::unique_ptr<SSL, decltype(&SSL_free)> m_ssl { nullptr, &SSL_free };
    std::unique_ptr<EC_KEY, decltype(&EC_KEY_free)> m_ecdh { nullptr, &EC_KEY_free };
    BIO* m_bioIn = nullptr;
    BIO* m_bioOut = nullptr;
};

class NX_WEBRTC_API AbstractDtlsDelegate
{
public:
    virtual ~AbstractDtlsDelegate();
    virtual void writeDtlsPacket(const char* data, int size) = 0;
    virtual bool onDtlsData(const uint8_t* data, int size) = 0;
};

} // namespace nx::webrtc
