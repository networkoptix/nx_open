#include <string>
#include <cstring>

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/sha.h>

#include "base64.h"
#include "license.h" 

namespace {

char networkOptixRSAPublicKey[] = "-----BEGIN PUBLIC KEY-----\n"
        "MFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBAN4wCk8ISwRsPH0Ev/ljnEygpL9n7PhA\n"
        "EwVi0AB6ht0hQ3sZUtM9UAGrszPJOzFfZlDB2hZ4HFyXfVZcbPxOdmECAwEAAQ==\n"
        "-----END PUBLIC KEY-----";

}

namespace LLUtil {

bool isSignatureMatch(const std::string &data, const std::string& signature, const std::string &publicKey) {
    unsigned char dataHash[SHA_DIGEST_LENGTH];

    SHA1((const unsigned char*)data.data(), data.size(), dataHash);

    // Load RSA public key
    BIO *bp = BIO_new_mem_buf(const_cast<char *>(publicKey.data()), (int)publicKey.size());
    RSA* publicRSAKey = PEM_read_bio_RSA_PUBKEY(bp, 0, 0, 0);
    BIO_free(bp);

    if (publicRSAKey == 0 || signature.size() != RSA_size(publicRSAKey))
        return false;

    // Decrypt data
    std::string decrypted;
    decrypted.resize(signature.size());

    int ret = RSA_public_decrypt((int)signature.size(), (const unsigned char*)signature.data(), (unsigned char*)decrypted.data(), publicRSAKey, RSA_PKCS1_PADDING);
    RSA_free(publicRSAKey);

    // Verify signature is correct
    return std::memcmp(decrypted.data(), dataHash, ret) == 0;
}

bool isNoptixTextSignatureMatch(const std::string &data, const std::string &signature) {
    std::string binarySig = base64_decode(signature);
    return isSignatureMatch(data, binarySig, networkOptixRSAPublicKey);
}

}
