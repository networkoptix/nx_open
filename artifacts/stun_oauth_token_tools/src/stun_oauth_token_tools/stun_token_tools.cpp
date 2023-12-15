/*
 * Copyright (C) 2011, 2012, 2013 Citrix Systems
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "stun_token_tools.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory.h>

#include <openssl/evp.h>
#include <openssl/ssl.h>

#if defined(_WIN32)
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
#endif

#include <nx/network/socket_common.h>
#include <nx/utils/base64.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/random_cryptographic_device.h>

namespace nx::network::stun {

const int kAes256KeySize = 32;

namespace {

static int evpEncryptUpdate(
    EVP_CIPHER_CTX* ctx, unsigned char* out, int* outl, const unsigned char* in, int inl)
{
    int cycle = 0;
    int outLen = 0;
    while ((outLen < inl) && (++cycle < 128))
    {
        int tmpOutl = 0;
        unsigned char* ptr = NULL;
        if (out)
            ptr = out + outLen;
        const int ret = EVP_EncryptUpdate(ctx, ptr, &tmpOutl, in + outLen, inl - outLen);
        outLen += tmpOutl;
        if (ret < 1)
            return ret;
    }
    *outl = outLen;
    return 1;
}

static int evpDecryptUpdate(
    EVP_CIPHER_CTX* ctx, unsigned char* out, int* outl, const unsigned char* in, int inl)
{
    int cycle = 0;
    int outLen = 0;
    while ((outLen < inl) && (++cycle < 128))
    {
        int tmpOutl = 0;
        unsigned char* ptr = NULL;
        if (out)
            ptr = out + outLen;
        const int ret = EVP_DecryptUpdate(ctx, ptr, &tmpOutl, in + outLen, inl - outLen);
        outLen += tmpOutl;
        if (ret < 1)
            return ret;
    }
    *outl = outLen;
    return 1;
}

static void generateRandomNonce(unsigned char* nonce, int size)
{
    const std::string s =
        nx::utils::random::generateName(nx::utils::random::CryptographicDevice::instance(), size);
    for (char c: s)
        *(nonce++) = c;
}

} // namespace

std::optional<std::string> encodeToken(
    const std::string& serverName, const std::string& key, const EncryptedBlock& encryptedBlock)
{
    if (key.size() < kAes256KeySize)
    {
        NX_ERROR(NX_SCOPE_TAG, "Wrong private key size.");
        return std::nullopt;
    }
    const EVP_CIPHER* cipher = EVP_aes_256_gcm();

    const int kMaxShaSize = 128;
    const int kMaxEncodedTokenSize = 1024;
    const int kOauthGcmNonceSize = 12;
    const int kOauthGcmTagSize = 16;

    if (encryptedBlock.keyLength > kMaxShaSize)
        return std::nullopt;

    unsigned char origField[kMaxEncodedTokenSize];
    memset(origField, 0, sizeof(origField));

    unsigned char nonce[kOauthGcmNonceSize];
    if (encryptedBlock.nonceLength == 0)
    {
        generateRandomNonce(nonce, kOauthGcmNonceSize);
    }
    else
    {
        memcpy(nonce, encryptedBlock.nonce, encryptedBlock.nonceLength);
    }

    size_t len = 0;

    *((uint16_t*) (origField + len)) = ntohs(kOauthGcmNonceSize);
    len += 2;

    memcpy(origField + len, nonce, kOauthGcmNonceSize);
    len += kOauthGcmNonceSize;

    *((uint16_t*) (origField + len)) = ntohs(encryptedBlock.keyLength);
    len += 2;

    memcpy(origField + len, encryptedBlock.macKey, encryptedBlock.keyLength);
    len += encryptedBlock.keyLength;

    const uint64_t ts = ntohll(encryptedBlock.timestamp);
    memcpy((origField + len), &ts, sizeof(ts));
    len += sizeof(ts);

    *((uint32_t*) (origField + len)) = ntohl(encryptedBlock.lifetime);
    len += 4;

    #if OPENSSL_VERSION_NUMBER < 0x10100000L
        EVP_CIPHER_CTX ctx;
        EVP_CIPHER_CTX* ctxp = &ctx;
    #else
        EVP_CIPHER_CTX* ctxp = EVP_CIPHER_CTX_new();
    #endif
    EVP_CIPHER_CTX_init(ctxp);

    if (1 != EVP_EncryptInit_ex(ctxp, cipher, NULL, NULL, NULL))
        return std::nullopt;

    EVP_CIPHER_CTX_set_padding(ctxp, 1);

    if (1 != EVP_CIPHER_CTX_ctrl(ctxp, EVP_CTRL_GCM_SET_IVLEN, kOauthGcmNonceSize, NULL))
        return std::nullopt;

    if (1 != EVP_EncryptInit_ex(ctxp, NULL, NULL, (const unsigned char*) key.c_str(), nonce))
        return std::nullopt;

    int outl = 0;
    const size_t snLen = serverName.size();

    if (1 != evpEncryptUpdate(ctxp, NULL, &outl, (uint8_t*) (serverName.c_str()), (int) snLen))
        return std::nullopt;

    outl = 0;
    char token[kMaxEncodedTokenSize];
    unsigned char* encodedField = (unsigned char*) token;
    memcpy(encodedField, origField, kOauthGcmNonceSize + 2);
    encodedField += kOauthGcmNonceSize + 2;
    unsigned char* startField = origField + kOauthGcmNonceSize + 2;
    len -= kOauthGcmNonceSize + 2;

    if (1 != evpEncryptUpdate(ctxp, encodedField, &outl, startField, (int) len))
        return std::nullopt;

    int tmp_outl = 0;
    EVP_EncryptFinal_ex(ctxp, encodedField + outl, &tmp_outl);
    outl += tmp_outl;

    EVP_CIPHER_CTX_ctrl(ctxp, EVP_CTRL_GCM_GET_TAG, kOauthGcmTagSize, encodedField + outl);
    outl += kOauthGcmTagSize;

    #if OPENSSL_VERSION_NUMBER < 0x10100000L
        EVP_CIPHER_CTX_cleanup(ctxp);
    #else
        EVP_CIPHER_CTX_free(ctxp);
    #endif

    char base64Encodedtoken[kMaxEncodedTokenSize];
    int tokenSize = nx::utils::toBase64(
        token, 2 + kOauthGcmNonceSize + outl, base64Encodedtoken, kMaxEncodedTokenSize);
    if (tokenSize <= 0)
        return std::nullopt;
    return std::string(base64Encodedtoken, tokenSize);
}

std::optional<EncryptedBlock> decodeToken(const std::string& serverName,
    const std::string& base64EncodedKey,
    const std::string& base64EncodedToken)
{
    const EVP_CIPHER* cipher = EVP_aes_256_gcm();

    std::string key = nx::utils::fromBase64(base64EncodedKey);
    if (key.size() != kAes256KeySize)
    {
        NX_WARNING(NX_SCOPE_TAG, "Wrong private key size. Key was resized to %1.", kAes256KeySize);
        key.resize(kAes256KeySize);
    }

    std::string token = nx::utils::fromBase64(base64EncodedToken);

    unsigned char snl[2];
    memcpy(snl, (const unsigned char*) (token.c_str()), 2);
    const unsigned char* csnl = snl;

    EncryptedBlock encryptedBlock;
    const uint16_t nonceLen = ntohs(*((const uint16_t*) csnl));
    encryptedBlock.nonceLength = nonceLen;

    constexpr int kOauthGcmTagSize = 16;
    size_t minEncodedFieldSize = 2 + 4 + 8 + nonceLen + 2 + kOauthGcmTagSize + 1;
    if (token.size() < minEncodedFieldSize)
    {
        NX_ERROR(NX_SCOPE_TAG, "%1: The token size is too small: %2", __FUNCTION__, token.size());
        return {};
    }

    const unsigned char* encodedField = (const unsigned char*) (token.c_str() + nonceLen + 2);
    const unsigned int encodedFieldSize =
        (unsigned int) token.size() - nonceLen - 2 - kOauthGcmTagSize;
    const unsigned char* nonce = ((const unsigned char*) token.c_str() + 2);
    memcpy(encryptedBlock.nonce, nonce, nonceLen);

    unsigned char tag[kOauthGcmTagSize];
    memcpy(tag,
        ((const unsigned char*) token.c_str()) + nonceLen + 2 + encodedFieldSize,
        sizeof(tag));

    const int kMaxEncodedOauthTokenSize = 1024;
    unsigned char decodedField[kMaxEncodedOauthTokenSize];

    #if OPENSSL_VERSION_NUMBER < 0x10100000L
        EVP_CIPHER_CTX ctx;
        EVP_CIPHER_CTX* ctxp = &ctx;
    #else
        EVP_CIPHER_CTX* ctxp = EVP_CIPHER_CTX_new();
    #endif
    EVP_CIPHER_CTX_init(ctxp);
    // Initialize the decryption operation.
    if (1 != EVP_DecryptInit_ex(ctxp, cipher, NULL, NULL, NULL))
    {
        NX_ERROR(NX_SCOPE_TAG, "%1: Cannot initialize decryption", __FUNCTION__);
        return std::nullopt;
    }
    // Set the IV length if the default value of 12 bytes (96 bits) is not appropriate.
    if (1 != EVP_CIPHER_CTX_ctrl(ctxp, EVP_CTRL_GCM_SET_IVLEN, nonceLen, NULL))
    {
        NX_ERROR(NX_SCOPE_TAG, "%1: Cannot set nonce length", __FUNCTION__);
        return std::nullopt;
    }
    // Initialize the key and IV.
    if (1 != EVP_DecryptInit_ex(ctxp, NULL, NULL, (const unsigned char*) key.c_str(), nonce))
    {
        NX_ERROR(NX_SCOPE_TAG, "%1: Cannot set nonce", __FUNCTION__);
        return std::nullopt;
    }

    // Set the expected tag value. A restriction in OpenSSL 1.0.1c and earlier required the tag
    // before any AAD or ciphertext.
    EVP_CIPHER_CTX_ctrl(ctxp, EVP_CTRL_GCM_SET_TAG, kOauthGcmTagSize, tag);

    int outl = 0;

    // Provide any AAD data. This can be called zero or more times as required.
    if (1 != evpDecryptUpdate(
        ctxp, NULL, &outl, (unsigned char*) serverName.c_str(), (int) serverName.size()))
    {
        NX_ERROR(NX_SCOPE_TAG,
            "%1: Cannot decrypt update server_name: %2, len=%3",
            __FUNCTION__,
            serverName,
            serverName.size());
        return std::nullopt;
    }

    if (1 != evpDecryptUpdate(ctxp, decodedField, &outl, encodedField, (int) encodedFieldSize))
    {
        NX_ERROR(NX_SCOPE_TAG, "%1: Cannot decrypt update", __FUNCTION__);
        return std::nullopt;
    }
    int tmpOutl = 0;
    if (EVP_DecryptFinal_ex(ctxp, decodedField + outl, &tmpOutl) < 1)
    {
    #if OPENSSL_VERSION_NUMBER < 0x10100000L
        EVP_CIPHER_CTX_cleanup(ctxp);
    #else
        EVP_CIPHER_CTX_free(ctxp);
    #endif
        NX_ERROR(NX_SCOPE_TAG, "%1: token integrity check failed", __FUNCTION__);
        return std::nullopt;
    }
    outl += tmpOutl;
    #if OPENSSL_VERSION_NUMBER < 0x10100000L
        EVP_CIPHER_CTX_cleanup(ctxp);
    #else
        EVP_CIPHER_CTX_free(ctxp);
    #endif

    size_t len = 0;

    encryptedBlock.keyLength = ntohs(*((uint16_t*) (decodedField + len)));
    len += 2;

    memcpy(encryptedBlock.macKey, decodedField + len, encryptedBlock.keyLength);
    len += encryptedBlock.keyLength;

    uint64_t ts;
    memcpy(&ts, (decodedField + len), sizeof(ts));
    encryptedBlock.timestamp = ntohll(ts);
    len += sizeof(ts);

    uint32_t lt;
    memcpy(&lt, (decodedField + len), sizeof(lt));
    encryptedBlock.lifetime = ntohl(lt);
    len += sizeof(lt);

    return encryptedBlock;
}

} // namespace nx::network::stun
