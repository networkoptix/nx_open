// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_dtls.h"

#include <openssl/err.h>

#include <nx/utils/log/log.h>

namespace nx::webrtc {

AbstractDtlsDelegate::~AbstractDtlsDelegate()
{
}

/*static*/ int Dtls::bioMethodNew(BIO *bio)
{
    BIO_set_init(bio, 1);
    BIO_set_data(bio, NULL);
    BIO_set_shutdown(bio, 0);
    return 1;
}

/*static*/ int Dtls::bioMethodFree(BIO *bio)
{
    if (!bio)
        return 0;
    BIO_set_data(bio, NULL);
    return 1;
}

/*static*/ int Dtls::bioMethodWrite(BIO *bio, const char *in, int inl)
{
    if (inl <= 0)
        return inl;
    auto dtls = reinterpret_cast<Dtls*>(BIO_get_data(bio));
    if (!dtls)
        return -1;
    NX_CRITICAL(dtls->m_ice);
    dtls->m_ice->writeDtlsPacket(in, inl); //< Maybe use callback instead?
    return inl;
}

/*static*/ long Dtls::bioMethodCtrl(BIO* /*bio*/, int cmd, long /*num*/, void * /*ptr*/)
{
    switch (cmd)
    {
    case BIO_CTRL_FLUSH:
        return 1;
    case BIO_CTRL_DGRAM_QUERY_MTU:
        return 0; //< SSL_OP_NO_QUERY_MTU must be set
    case BIO_CTRL_WPENDING:
    case BIO_CTRL_PENDING:
        return 0;
    default:
        break;
    }
    return 0;
}

/*static*/ int Dtls::certificateCallback(int /*preverify_ok*/, X509_STORE_CTX* ctx)
{
    SSL* ssl = (SSL*) X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
    Dtls* dtls = (Dtls*) SSL_get_app_data(ssl);

    X509* x509 = X509_STORE_CTX_get_current_cert(ctx);
    nx::network::ssl::CertificateView remote(x509);
    std::vector<uint8_t> remoteHash;
    switch (dtls->m_fingerprint.type)
    {
    case Fingerprint::Type::sha256:
        remoteHash = remote.sha256();
        break;
    case Fingerprint::Type::sha1:
        remoteHash = remote.sha1();
        break;
    case Fingerprint::Type::none:
        NX_ASSERT(false, "%1: Fingerprint can't be absent", dtls->m_id);
        return 0;
    default:
        NX_WARNING(dtls, "%1: Fingerprint type %2 not supported", dtls->m_id, dtls->m_fingerprint.type);
        return 0;
    }
    return remoteHash == dtls->m_fingerprint.hash ? 1 : 0;
}

/*static*/ void Dtls::infoCallback(const SSL* /*ssl*/, int /*where*/, int /*ret*/)
{
    // XXX Collecting additional info about DTLS connection state.
}

bool Dtls::initDtls()
{
    /* XXX Refactor me please.
     * Context and Methods should be created once.
     * Each connection should make only personal transport index and SSL object.
     * */

    // 1. Setup methods
    m_bioMethods.reset(BIO_meth_new(BIO_TYPE_BIO, "DTLS writer"));
    BIO_meth_set_create(m_bioMethods.get(), bioMethodNew);
    BIO_meth_set_destroy(m_bioMethods.get(), bioMethodFree);
    BIO_meth_set_write(m_bioMethods.get(), bioMethodWrite);
    BIO_meth_set_ctrl(m_bioMethods.get(), bioMethodCtrl);

    // 2. Context
    m_sslContext.reset(SSL_CTX_new(DTLS_method()));

    /* RFC 8261: SCTP performs segmentation and reassembly based on the path MTU.
     * Therefore, the DTLS layer MUST NOT use any compression algorithm.
     * See https://www.rfc-editor.org/rfc/rfc8261.html#section-5
     * RFC 8827: Implementations MUST NOT implement DTLS renegotiation
     * See https://www.rfc-editor.org/rfc/rfc8827.html#section-6.5
     */
    SSL_CTX_set_options(m_sslContext.get(),
        SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION | SSL_OP_NO_QUERY_MTU | SSL_OP_NO_RENEGOTIATION);
    SSL_CTX_set_min_proto_version(m_sslContext.get(), DTLS1_VERSION);
    SSL_CTX_set_read_ahead(m_sslContext.get(), 1);
    SSL_CTX_set_quiet_shutdown(m_sslContext.get(), 1);
    SSL_CTX_set_info_callback(m_sslContext.get(), infoCallback);

    SSL_CTX_set_verify(m_sslContext.get(), SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
        certificateCallback);
    SSL_CTX_set_verify_depth(m_sslContext.get(), 1);
    if (SslError::failed == sslErrorCheck(SSL_CTX_set_cipher_list(
        m_sslContext.get(), "ALL:!LOW:!EXP:!RC4:!MD5:@STRENGTH"), "failed to set SSL priorities"))
    {
        return false;
    }

    // 3. Set certificate and key
    SSL_CTX_use_certificate(m_sslContext.get(), m_pem.certificate().x509());
    SSL_CTX_use_PrivateKey(m_sslContext.get(), m_pem.privateKey().get());
    if (SslError::failed == sslErrorCheck(SSL_CTX_check_private_key(m_sslContext.get()),
        "SSL local private key check failed"))
    {
        return false;
    }

    // 4. SSL
    m_ssl.reset(SSL_new(m_sslContext.get()));
    if (m_type == Type::server)
        SSL_set_accept_state(m_ssl.get());
    else
        SSL_set_connect_state(m_ssl.get());
    SSL_set_mtu(m_ssl.get(), 1280 - 8 - 40); //< From libdatachannel

    // 5. Bind to current object
    SSL_set_app_data(m_ssl.get(), this);

    // 6. BIOs
    m_bioIn = BIO_new(BIO_s_mem());
    m_bioOut = BIO_new(m_bioMethods.get());
    BIO_set_mem_eof_return(m_bioIn, -1);
    BIO_set_data(m_bioOut, this);
    SSL_set_bio(m_ssl.get(), m_bioIn, m_bioOut);

    // 7. Some another stuff
    m_ecdh.reset(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
    SSL_set_options(m_ssl.get(), SSL_OP_SINGLE_ECDH_USE);
    SSL_set_tmp_ecdh(m_ssl.get(), m_ecdh.get());

    // RFC 8827: The DTLS-SRTP protection profile SRTP_AES128_CM_HMAC_SHA1_80 MUST be supported
    // See https://www.rfc-editor.org/rfc/rfc8827.html#section-6.5 Warning:
    // SSL_set_tlsext_use_srtp() returns 0 on success and 1 on error
    if (SSL_set_tlsext_use_srtp(m_ssl.get(), "SRTP_AES128_CM_SHA1_80") != 0)
        return false;
    return true;

}

Dtls::SslError Dtls::sslErrorCheck(int success, const std::string& message)
{
    int sslError = 0;
    if (m_ssl)
        sslError = SSL_get_error(m_ssl.get(), success);

    unsigned long lastError = ERR_peek_last_error();
    ERR_clear_error();

    if (!m_ssl)
    {
        if (success > 0)
            return SslError::ok;
        return sslDumpError(sslErrorString(lastError), message);
    }
    else
    {
        if (sslError == SSL_ERROR_NONE)
            return SslError::ok;
        if (sslError == SSL_ERROR_ZERO_RETURN)
            return sslDumpError("peer closed connection", message);
        if (sslError == SSL_ERROR_SYSCALL)
            return sslDumpError("fatal I/O error", message);
        if (sslError == SSL_ERROR_SSL)
            return sslDumpError(sslErrorString(lastError), message);
        // SSL_ERROR_WANT_READ and SSL_ERROR_WANT_WRITE end up here.
        return SslError::continue_;
    }
}

Dtls::SslError Dtls::sslDumpError(const std::string& error, const std::string& message)
{
    NX_DEBUG(
        this,
        "SSL check failed: %1 / %2 for %3",
        message,
        error,
        m_id);
    return SslError::failed;
}

std::string Dtls::sslErrorString(unsigned long lastError)
{
    constexpr int kBufferSize = 256;
    char buffer[kBufferSize];
    ERR_error_string_n(lastError, buffer, kBufferSize);
    return std::string(buffer);
}

Dtls::Status Dtls::writePacket(const uint8_t* data, int size, AbstractDtlsDelegate* ice)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_ice = ice;
    if (!m_ssl.get() || m_status != Status::streaming)
        return Status::error;

    return SslError::failed != sslErrorCheck(SSL_write(m_ssl.get(), data, size), "Write to SSL failed")
        ? Status::streaming
        : Status::error;
}

Dtls::Status Dtls::handlePacket(const uint8_t* data, int size, AbstractDtlsDelegate* ice)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_ice = ice;

    // 1. Init.
    if (!m_ssl.get() && !initDtls())
    {
        NX_DEBUG(this, "Init failed for %1", m_id);
        return Status::error;
    }

    // 2. DTLS handshake
    if (size != 0 && BIO_write(m_bioIn, data, size) != size)
    {
        NX_DEBUG(this, "BIO_write() failed for %1", m_id);
        return Status::error;
    }

    if (m_status == Status::streaming)
    {
        // Handshake is finished, it's some data via DTLS.
        constexpr size_t kBufferSize = 4096;
        uint8_t buffer[kBufferSize];
        int ret = SSL_read(m_ssl.get(), buffer, kBufferSize);
        if (SslError::failed == sslErrorCheck(ret, "SSL_read() failed"))
            return Status::streaming; //< Not a fatal but strange.
        if (!m_ice->onDtlsData(buffer, ret))
            return Status::error;
        return Status::streaming;
    }

    SslError sslError = sslErrorCheck(SSL_do_handshake(m_ssl.get()), "Handshake failed");

    if (SslError::failed == sslError)
        return Status::error;

    if (SslError::continue_ == sslError)
    {
        NX_VERBOSE(this, "Handshake continuation for %1", m_id);
        return m_status = Status::handshake; //< Handshake is not finished, need for more chunks of data
    }

    // 3. Master key & salt extraction
    static const std::string extractorLabel("EXTRACTOR-dtls_srtp");
    constexpr size_t kMaterialLength = rtsp::kSrtpKeyAndSaltLen * 2;
    uint8_t material[kMaterialLength];

    // Returns 1 on success, 0 or -1 on failure
    if (SSL_export_keying_material(m_ssl.get(), material, kMaterialLength,
        extractorLabel.c_str(), extractorLabel.size(), nullptr, 0, 0) <= 0)
    {
        NX_DEBUG(this, "Can't export keying material for %1", m_id);
        return Status::error;
    }
    // Order is client key, server key, client salt, and server salt
    memcpy(m_encryptionData.clientKeyAndSalt, material, rtsp::kSrtpAes128KeyLen);
    memcpy(m_encryptionData.serverKeyAndSalt, material + rtsp::kSrtpAes128KeyLen, rtsp::kSrtpAes128KeyLen);
    memcpy(m_encryptionData.clientKeyAndSalt + rtsp::kSrtpAes128KeyLen,
        material + rtsp::kSrtpAes128KeyLen * 2, rtsp::kSrtpSaltLen);
    memcpy(m_encryptionData.serverKeyAndSalt + rtsp::kSrtpAes128KeyLen,
        material + rtsp::kSrtpAes128KeyLen * 2 + rtsp::kSrtpSaltLen, rtsp::kSrtpSaltLen);

    NX_DEBUG(this, "Handshake successful for session %1", m_id);
    return m_status = Status::streaming;
}

rtsp::EncryptionData Dtls::encryptionData() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_encryptionData;
}

void Dtls::setFingerprint(const Fingerprint& fingerprint)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_fingerprint = fingerprint;
}

} // namespace nx::webrtc
