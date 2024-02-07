// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cryptographic_hash.h"

#include <cstdio>

#include <QtCore/QIODevice>

#include <openssl/evp.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

namespace nx::utils {

// -------------------------------------------------------------------------- //
// QnCryptographicHashPrivate
// -------------------------------------------------------------------------- //
class QnCryptographicHashPrivate
{
public:
    QnCryptographicHashPrivate() {}
    virtual ~QnCryptographicHashPrivate() {}

    virtual void init() = 0;
    virtual void update(const char *data, int length) = 0;
    virtual void final(unsigned char *result) = 0;
    virtual int size() const = 0;
    virtual QnCryptographicHashPrivate *clone() const = 0;

private:
    friend class QnCryptographicHash;

    QByteArray m_result;
};

class QnMd4CryptographicHashPrivate: public QnCryptographicHashPrivate
{
public:
    virtual void init() override { MD4_Init(&ctx); }
    virtual void update(const char *data, int length) override { MD4_Update(&ctx, data, length); }
    virtual void final(unsigned char *result) override { MD4_Final(result, &ctx); }
    virtual int size() const override { return MD4_DIGEST_LENGTH; }
    virtual QnCryptographicHashPrivate *clone() const override { return new QnMd4CryptographicHashPrivate(*this); }

private:
    MD4_CTX ctx;
};

class QnMd5CryptographicHashPrivate: public QnCryptographicHashPrivate
{
public:
    virtual void init() override { MD5_Init(&ctx); }
    virtual void update(const char *data, int length) override { MD5_Update(&ctx, data, length); }
    virtual void final(unsigned char *result) override { MD5_Final(result, &ctx); }
    virtual int size() const override { return MD5_DIGEST_LENGTH; }
    virtual QnCryptographicHashPrivate *clone() const override { return new QnMd5CryptographicHashPrivate(*this); }

private:
    MD5_CTX ctx;
};

class QnSha1CryptographicHashPrivate: public QnCryptographicHashPrivate
{
public:
    virtual void init() override { SHA1_Init(&ctx); }
    virtual void update(const char *data, int length) override { SHA1_Update(&ctx, data, length); }
    virtual void final(unsigned char *result) override { SHA1_Final(result, &ctx); }
    virtual int size() const override { return SHA_DIGEST_LENGTH; }
    virtual QnCryptographicHashPrivate *clone() const override { return new QnSha1CryptographicHashPrivate(*this); }

private:
    SHA_CTX ctx;
};

class QnSha256CryptographicHashPrivate: public QnCryptographicHashPrivate
{
public:
    virtual void init() override { SHA256_Init(&ctx); }
    virtual void update(const char *data, int length) override { SHA256_Update(&ctx, data, length); }
    virtual void final(unsigned char *result) override { SHA256_Final(result, &ctx); }
    virtual int size() const override { return SHA256_DIGEST_LENGTH; }
    virtual QnCryptographicHashPrivate *clone() const override { return new QnSha256CryptographicHashPrivate(*this); }

private:
    SHA256_CTX ctx;
};

using EvpFunc = const EVP_MD*(*)();

class EvpDigestCalculator: public QnCryptographicHashPrivate
{
public:
    EvpDigestCalculator(EvpFunc evpFunc, int digestLen):
        m_evpFunc(evpFunc),
        m_digestLen(digestLen)
    {
    }

    ~EvpDigestCalculator()
    {
        if (m_ctx)
        {
            EVP_MD_CTX_destroy(m_ctx);
            m_ctx = nullptr;
        }
    }

    virtual void init() override
    {
        m_ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(m_ctx, m_evpFunc(), nullptr);
    }

    virtual void update(const char* data, int length) override
    {
        EVP_DigestUpdate(m_ctx, data, length);
    }

    virtual void final(unsigned char* result) override
    {
        unsigned int len = m_digestLen;
        EVP_DigestFinal_ex(m_ctx, result, &len);
    }

    virtual int size() const override { return m_digestLen; }

    virtual QnCryptographicHashPrivate* clone() const override
    {
        EvpDigestCalculator* obj = new EvpDigestCalculator(m_evpFunc, m_digestLen);
        obj->m_ctx = EVP_MD_CTX_new();
        EVP_MD_CTX_copy_ex(obj->m_ctx, m_ctx);
        return obj;
    }

private:
    EvpFunc m_evpFunc = nullptr;
    int m_digestLen = 0;
    EVP_MD_CTX* m_ctx = nullptr;
};

class QnSha3256CryptographicHashPrivate: public EvpDigestCalculator
{
public:
    QnSha3256CryptographicHashPrivate():
        EvpDigestCalculator(EVP_sha3_256, SHA256_DIGEST_LENGTH)
    {
    }
};

class QnSha3512CryptographicHashPrivate: public EvpDigestCalculator
{
public:
    QnSha3512CryptographicHashPrivate():
        EvpDigestCalculator(EVP_sha3_512, SHA512_DIGEST_LENGTH)
    {
    }
};

// -------------------------------------------------------------------------- //
// QnCryptographicHash
// -------------------------------------------------------------------------- //
QnCryptographicHash::QnCryptographicHash(Algorithm algorithm)
{
    switch (algorithm)
    {
    case Md4:
        d.reset(new QnMd4CryptographicHashPrivate());
        break;
    case Md5:
        d.reset(new QnMd5CryptographicHashPrivate());
        break;
    case Sha1:
        d.reset(new QnSha1CryptographicHashPrivate());
        break;
    case Sha256:
        d.reset(new QnSha256CryptographicHashPrivate());
        break;
    case Sha3_256:
        d.reset(new QnSha3256CryptographicHashPrivate());
        break;
    case Sha3_512:
        d.reset(new QnSha3512CryptographicHashPrivate());
        break;
    default:
        std::printf("%s: Invalid cryptographic hash algorithm %d.\n",
            Q_FUNC_INFO, static_cast<int>(algorithm));
        d.reset(new QnMd5CryptographicHashPrivate());
        break;
    }

    d->init();
}

QnCryptographicHash::~QnCryptographicHash()
{
    return;
}

QnCryptographicHash::QnCryptographicHash(const QnCryptographicHash &other)
{
    d.reset(other.d->clone());
}

QnCryptographicHash &QnCryptographicHash::operator=(const QnCryptographicHash &other)
{
    this->~QnCryptographicHash();
    new (this) QnCryptographicHash(other);
    return *this;
}

void QnCryptographicHash::addData(const char *data, int length)
{
    d->update(data, length);
}

void QnCryptographicHash::addData(const QByteArray& data)
{
    d->update(data.data(), data.size());
}

void QnCryptographicHash::addData(const nx::Buffer& data)
{
    d->update(data.data(), (int) data.size());
}

void QnCryptographicHash::addData(const std::string_view& data)
{
    d->update(data.data(), (int) data.size());
}

void QnCryptographicHash::addData(const std::string& data)
{
    d->update(data.data(), (int) data.size());
}

void QnCryptographicHash::addData(const char* data)
{
    d->update(data, (int) std::strlen(data));
}

bool QnCryptographicHash::addData(QIODevice* device)
{
    if (!device->isReadable())
        return false;

    if (!device->isOpen())
        return false;

    char buffer[1024];
    qint64 length;

    while ((length = device->read(buffer, sizeof(buffer))) > 0)
        addData(buffer, (int) length);

    return device->atEnd();
}

void QnCryptographicHash::reset()
{
    d->m_result = QByteArray();
    d->init();
}

QByteArray QnCryptographicHash::result() const
{
    if (d->m_result.isEmpty())
    {
        d->m_result.resize(d->size());
        d->final(reinterpret_cast<unsigned char*>(d->m_result.data()));
    }

    return d->m_result;
}

QByteArray QnCryptographicHash::hash(const QByteArray &data, Algorithm algorithm)
{
    QnCryptographicHash hash(algorithm);
    hash.addData(data);
    return hash.result();
}

//-------------------------------------------------------------------------------------------------

template<typename Hasher, typename Output, typename Input>
Output calculateHash(const Input& data)
{
    Hasher hasher;
    hasher.init();
    hasher.update(data.data(), data.size());

    Output hash(hasher.size(), '\0');
    hasher.final(reinterpret_cast<unsigned char*>(hash.data()));
    return hash;
}

std::string sha1(const std::string_view& data)
{
    return calculateHash<QnSha1CryptographicHashPrivate, std::string>(data);
}

QByteArray sha3_256(const QByteArray& data)
{
    return calculateHash<QnSha3256CryptographicHashPrivate, QByteArray>(data);
}

std::string sha3_256(const std::string_view& data)
{
    return calculateHash<QnSha3256CryptographicHashPrivate, std::string>(data);
}

QByteArray sha3_512(const QByteArray& data)
{
    return calculateHash<QnSha3512CryptographicHashPrivate, QByteArray>(data);
}

std::string sha3_512(const std::string_view& data)
{
    return calculateHash<QnSha3512CryptographicHashPrivate, std::string>(data);
}

} // namespace nx::utils
