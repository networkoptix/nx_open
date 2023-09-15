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

#include "ns_turn_msg.h"

namespace nx::network::stun {

const int kAes256KeySize = 32;

namespace {

static int evpEncryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl, const unsigned char *in, int inl) {
  int cycle = 0;
  int out_len = 0;
  while ((out_len < inl) && (++cycle < 128)) {
    int tmp_outl = 0;
    unsigned char *ptr = NULL;
    if (out)
      ptr = out + out_len;
    int ret = EVP_EncryptUpdate(ctx, ptr, &tmp_outl, in + out_len, inl - out_len);
    out_len += tmp_outl;
    if (ret < 1)
      return ret;
  }
  *outl = out_len;
  return 1;
}

static int evpDecryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl, const unsigned char *in, int inl) {
  int cycle = 0;
  int out_len = 0;
  while ((out_len < inl) && (++cycle < 128)) {
    int tmp_outl = 0;
    unsigned char *ptr = NULL;
    if (out)
      ptr = out + out_len;
    int ret = EVP_DecryptUpdate(ctx, ptr, &tmp_outl, in + out_len, inl - out_len);
    out_len += tmp_outl;
    if (ret < 1)
      return ret;
  }
  *outl = out_len;
  return 1;
}

static void generateRandomNonce(unsigned char *nonce, size_t sz) {
  if (!RAND_bytes(nonce, (int)sz)) {
    size_t i;
    for (i = 0; i < sz; ++i) {
      nonce[i] = (unsigned char)turn_random();
    }
  }
}

} // namespace

std::optional<std::string> encodeToken(
    const std::string& serverName, const std::string& key, const EncryptedBlock& encryptedBlock)
{
  if (server_name && etoken && key && dtoken && (dtoken->enc_block.key_length <= MAXSHASIZE)) {

    unsigned char orig_field[MAX_ENCODED_OAUTH_TOKEN_SIZE];
    memset(orig_field, 0, sizeof(orig_field));

    unsigned char nonce[OAUTH_GCM_NONCE_SIZE];
    if (nonce0) {
      memcpy(nonce, nonce0, sizeof(nonce));
    } else {
      generate_random_nonce(nonce, sizeof(nonce));
    }

    size_t len = 0;

    *((uint16_t *)(orig_field + len)) = nswap16(OAUTH_GCM_NONCE_SIZE);
    len += 2;

    memcpy(orig_field + len, nonce, OAUTH_GCM_NONCE_SIZE);
    len += OAUTH_GCM_NONCE_SIZE;

    *((uint16_t *)(orig_field + len)) = nswap16(dtoken->enc_block.key_length);
    len += 2;

    memcpy(orig_field + len, dtoken->enc_block.mac_key, dtoken->enc_block.key_length);
    len += dtoken->enc_block.key_length;

    uint64_t ts = nswap64(dtoken->enc_block.timestamp);
    memcpy((orig_field + len), &ts, sizeof(ts));
    len += sizeof(ts);

    *((uint32_t *)(orig_field + len)) = nswap32(dtoken->enc_block.lifetime);
    len += 4;

    const EVP_CIPHER *cipher = get_cipher_type(key->as_rs_alg);
    if (!cipher)
      return -1;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_CIPHER_CTX ctx;
    EVP_CIPHER_CTX *ctxp = &ctx;
#else
    EVP_CIPHER_CTX *ctxp = EVP_CIPHER_CTX_new();
#endif
    EVP_CIPHER_CTX_init(ctxp);

    /* Initialize the encryption operation. */
    if (1 != EVP_EncryptInit_ex(ctxp, cipher, NULL, NULL, NULL))
      return -1;

    EVP_CIPHER_CTX_set_padding(ctxp, 1);

    /* Set IV length if default 12 bytes (96 bits) is not appropriate */
    if (1 != EVP_CIPHER_CTX_ctrl(ctxp, EVP_CTRL_GCM_SET_IVLEN, OAUTH_GCM_NONCE_SIZE, NULL))
      return -1;

    /* Initialize key and IV */
    if (1 != EVP_EncryptInit_ex(ctxp, NULL, NULL, (const unsigned char *)key->as_rs_key, nonce))
      return -1;

    int outl = 0;
    size_t sn_len = strlen((const char *)server_name);

    /* Provide any AAD data. This can be called zero or more times as
     * required
     */
    if (1 != my_EVP_EncryptUpdate(ctxp, NULL, &outl, server_name, (int)sn_len))
      return -1;

    outl = 0;
    unsigned char *encoded_field = (unsigned char *)etoken->token;
    memcpy(encoded_field, orig_field, OAUTH_GCM_NONCE_SIZE + 2);
    encoded_field += OAUTH_GCM_NONCE_SIZE + 2;
    unsigned char *start_field = orig_field + OAUTH_GCM_NONCE_SIZE + 2;
    len -= OAUTH_GCM_NONCE_SIZE + 2;

    if (1 != my_EVP_EncryptUpdate(ctxp, encoded_field, &outl, start_field, (int)len))
      return -1;

    int tmp_outl = 0;
    EVP_EncryptFinal_ex(ctxp, encoded_field + outl, &tmp_outl);
    outl += tmp_outl;

    EVP_CIPHER_CTX_ctrl(ctxp, EVP_CTRL_GCM_GET_TAG, OAUTH_GCM_TAG_SIZE, encoded_field + outl);
    outl += OAUTH_GCM_TAG_SIZE;

    etoken->size = 2 + OAUTH_GCM_NONCE_SIZE + outl;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_CIPHER_CTX_cleanup(ctxp);
#else
    EVP_CIPHER_CTX_free(ctxp);
#endif

    return 0;
  }
  return -1;
}

std::optional<EncryptedBlock> decodeToken(const std::string& serverName,
    const std::string& base64EncodedKey,
    const std::string& base64EncodedToken)
{
  if (server_name && etoken && key && dtoken) {

    unsigned char snl[2];
    memcpy(snl, (const unsigned char *)(etoken->token), 2);
    const unsigned char *csnl = snl;

    uint16_t nonce_len = nswap16(*((const uint16_t *)csnl));
    dtoken->enc_block.nonce_length = nonce_len;

    size_t min_encoded_field_size = 2 + 4 + 8 + nonce_len + 2 + OAUTH_GCM_TAG_SIZE + 1;
    if (etoken->size < min_encoded_field_size) {
      OAUTH_ERROR("%s: token size too small: %d\n", __FUNCTION__, (int)etoken->size);
      return -1;
    }

    const unsigned char *encoded_field = (const unsigned char *)(etoken->token + nonce_len + 2);
    unsigned int encoded_field_size = (unsigned int)etoken->size - nonce_len - 2 - OAUTH_GCM_TAG_SIZE;
    const unsigned char *nonce = ((const unsigned char *)etoken->token + 2);
    memcpy(dtoken->enc_block.nonce, nonce, nonce_len);

    unsigned char tag[OAUTH_GCM_TAG_SIZE];
    memcpy(tag, ((const unsigned char *)etoken->token) + nonce_len + 2 + encoded_field_size, sizeof(tag));

    unsigned char decoded_field[MAX_ENCODED_OAUTH_TOKEN_SIZE];

    const EVP_CIPHER *cipher = get_cipher_type(key->as_rs_alg);
    if (!cipher) {
      OAUTH_ERROR("%s: Cannot find cipher for algorithm: %d\n", __FUNCTION__, (int)key->as_rs_alg);
      return -1;
    }

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_CIPHER_CTX ctx;
    EVP_CIPHER_CTX *ctxp = &ctx;
#else
    EVP_CIPHER_CTX *ctxp = EVP_CIPHER_CTX_new();
#endif
    EVP_CIPHER_CTX_init(ctxp);
    /* Initialize the decryption operation. */
    if (1 != EVP_DecryptInit_ex(ctxp, cipher, NULL, NULL, NULL)) {
      OAUTH_ERROR("%s: Cannot initialize decryption\n", __FUNCTION__);
      return -1;
    }

    // EVP_CIPHER_CTX_set_padding(&ctx,1);

    /* Set IV length if default 12 bytes (96 bits) is not appropriate */
    if (1 != EVP_CIPHER_CTX_ctrl(ctxp, EVP_CTRL_GCM_SET_IVLEN, nonce_len, NULL)) {
      OAUTH_ERROR("%s: Cannot set nonce length\n", __FUNCTION__);
      return -1;
    }

    /* Initialize key and IV */
    if (1 != EVP_DecryptInit_ex(ctxp, NULL, NULL, (const unsigned char *)key->as_rs_key, nonce)) {
      OAUTH_ERROR("%s: Cannot set nonce\n", __FUNCTION__);
      return -1;
    }

    /* Set expected tag value. A restriction in OpenSSL 1.0.1c and earlier
      +         * required the tag before any AAD or ciphertext */
    EVP_CIPHER_CTX_ctrl(ctxp, EVP_CTRL_GCM_SET_TAG, OAUTH_GCM_TAG_SIZE, tag);

    int outl = 0;
    size_t sn_len = strlen((const char *)server_name);

    /* Provide any AAD data. This can be called zero or more times as
     * required
     */
    if (1 != my_EVP_DecryptUpdate(ctxp, NULL, &outl, server_name, (int)sn_len)) {
      OAUTH_ERROR("%s: Cannot decrypt update server_name: %s, len=%d\n", __FUNCTION__, server_name, (int)sn_len);
      return -1;
    }
    if (1 != my_EVP_DecryptUpdate(ctxp, decoded_field, &outl, encoded_field, (int)encoded_field_size)) {
      OAUTH_ERROR("%s: Cannot decrypt update\n", __FUNCTION__);
      return -1;
    }

    int tmp_outl = 0;
    if (EVP_DecryptFinal_ex(ctxp, decoded_field + outl, &tmp_outl) < 1) {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
      EVP_CIPHER_CTX_cleanup(ctxp);
#else
      EVP_CIPHER_CTX_free(ctxp);
#endif
      OAUTH_ERROR("%s: token integrity check failed\n", __FUNCTION__);
      return -1;
    }
    outl += tmp_outl;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_CIPHER_CTX_cleanup(ctxp);
#else
    EVP_CIPHER_CTX_free(ctxp);
#endif

    size_t len = 0;

    dtoken->enc_block.key_length = nswap16(*((uint16_t *)(decoded_field + len)));
    len += 2;

    memcpy(dtoken->enc_block.mac_key, decoded_field + len, dtoken->enc_block.key_length);
    len += dtoken->enc_block.key_length;

    uint64_t ts;
    memcpy(&ts, (decoded_field + len), sizeof(ts));
    dtoken->enc_block.timestamp = nswap64(ts);
    len += sizeof(ts);

    uint32_t lt;
    memcpy(&lt, (decoded_field + len), sizeof(lt));
    dtoken->enc_block.lifetime = nswap32(lt);
    len += sizeof(lt);

    return 0;
  }
  return -1;
}

} // namespace nx::network::stun
