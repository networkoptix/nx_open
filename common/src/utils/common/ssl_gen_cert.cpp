#include <openssl/x509.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <openssl/applink.c>
#ifdef __cplusplus
}
#endif

int generateSslCertificate(const char *outpath, const char *commonName, const char *country, const char *companyName)
{
    OpenSSL_add_all_algorithms();
    CRYPTO_malloc_init();

    int returnValue = -1;

    EVP_PKEY * pkey = NULL;
    RSA * rsa = NULL;
    X509 * x509 = NULL;
    FILE * f = NULL;

    pkey = EVP_PKEY_new();
    if (!pkey) {
        goto finish;
    }

    rsa = RSA_generate_key(
                           2048,   /* number of bits for the key - 2048 is a sensible value */
                           RSA_F4, /* exponent - RSA_F4 is defined as 0x10001L */
                           NULL,   /* callback - can be NULL if we aren't displaying progress */
                           NULL    /* callback argument - not needed in this case */
                           );

    if (!rsa) {
        goto finish;
    }

    EVP_PKEY_assign_RSA(pkey, rsa);

    x509 = X509_new();

    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);

    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 157680000L); // 5 years

    X509_set_pubkey(x509, pkey);

    X509_NAME * name;
    name = X509_get_subject_name(x509);

    X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_ASC,
                               (unsigned char *)country, -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_ASC,
                               (unsigned char *)companyName, -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               (unsigned char *)commonName, -1, -1, 0);

    X509_set_issuer_name(x509, name);
    X509_sign(x509, pkey, EVP_sha1());

    f = fopen(outpath, "wb");
    if (!f) {
        goto finish;
    }

    PEM_write_PrivateKey(
                         f,                  /* write the key to the file we've opened */
                         pkey,               /* our key from earlier */
                         NULL, /* default cipher for encrypting the key on disk */
                         NULL,       /* passphrase required for decrypting the key on disk */
                         0,                 /* length of the passphrase string */
                         NULL,               /* callback for requesting a password */
                         NULL                /* data to pass to the callback */
                         );

    PEM_write_X509(
                   f,   /* write the certificate to the file we've opened */
                   x509 /* our certificate */
                   );


    returnValue = 0;

 finish:
    if (f)
        fclose(f);

    if (x509)
        X509_free(x509);

    if (pkey)
        EVP_PKEY_free(pkey);

    return returnValue;
}
