#include "cryptographic_hash.h"

#include <cstdio>

#include <QtCore/QIODevice>

#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

namespace nx {
namespace utils {

// -------------------------------------------------------------------------- //
// QnCryptographicHashPrivate
// -------------------------------------------------------------------------- //
class QnCryptographicHashPrivate {
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

    QByteArray result;
};

class QnMd4CryptographicHashPrivate : public QnCryptographicHashPrivate {
public:
    virtual void init() override { MD4_Init(&ctx); }
    virtual void update(const char *data, int length) override { MD4_Update(&ctx, data, length); }
    virtual void final(unsigned char *result) override { MD4_Final(result, &ctx); }
    virtual int size() const override { return MD4_DIGEST_LENGTH; }
    virtual QnCryptographicHashPrivate *clone() const { return new QnMd4CryptographicHashPrivate(*this); }

private:
    MD4_CTX ctx;
};

class QnMd5CryptographicHashPrivate : public QnCryptographicHashPrivate {
public:
    virtual void init() override { MD5_Init(&ctx); }
    virtual void update(const char *data, int length) override { MD5_Update(&ctx, data, length); }
    virtual void final(unsigned char *result) override { MD5_Final(result, &ctx); }
    virtual int size() const override { return MD5_DIGEST_LENGTH; }
    virtual QnCryptographicHashPrivate *clone() const { return new QnMd5CryptographicHashPrivate(*this); }

private:
    MD5_CTX ctx;
};

class QnSha1CryptographicHashPrivate : public QnCryptographicHashPrivate {
public:
    virtual void init() override { SHA1_Init(&ctx); }
    virtual void update(const char *data, int length) override { SHA1_Update(&ctx, data, length); }
    virtual void final(unsigned char *result) override { SHA1_Final(result, &ctx); }
    virtual int size() const override { return SHA_DIGEST_LENGTH; }
    virtual QnCryptographicHashPrivate *clone() const { return new QnSha1CryptographicHashPrivate(*this); }

private:
    SHA_CTX ctx;
};


// -------------------------------------------------------------------------- //
// QnCryptographicHash
// -------------------------------------------------------------------------- //
QnCryptographicHash::QnCryptographicHash(Algorithm algorithm) {
    switch (algorithm) {
    case Md4:
        d.reset(new QnMd4CryptographicHashPrivate());
        break;
    case Md5:
        d.reset(new QnMd5CryptographicHashPrivate());
        break;
    case Sha1:
        d.reset(new QnSha1CryptographicHashPrivate());
        break;
    default:
        std::printf("%s: Invalid cryptographic hash algorithm %d.\n", Q_FUNC_INFO, static_cast<int>(algorithm));
        d.reset(new QnMd5CryptographicHashPrivate());
        break;
    }

    d->init();
}

QnCryptographicHash::~QnCryptographicHash() {
    return;
}

QnCryptographicHash::QnCryptographicHash(const QnCryptographicHash &other) {
    d.reset(other.d->clone());
}

QnCryptographicHash &QnCryptographicHash::operator=(const QnCryptographicHash &other) {
    this->~QnCryptographicHash();
    new (this) QnCryptographicHash(other);
    return *this;
}

void QnCryptographicHash::addData(const char *data, int length) {
    d->update(data, length);
}

void QnCryptographicHash::addData(const QByteArray &data) {
    d->update(data.data(), data.size());
}

bool QnCryptographicHash::addData(QIODevice* device) {
    if (!device->isReadable())
        return false;

    if (!device->isOpen())
        return false;

    char buffer[1024];
    int length;

    while ((length = device->read(buffer, sizeof(buffer))) > 0)
        addData(buffer, length);

    return device->atEnd();
}

void QnCryptographicHash::reset() {
    d->result = QByteArray();
    d->init();
}

QByteArray QnCryptographicHash::result() const {
    if (d->result.isEmpty()) {
        d->result.resize(d->size());
        d->final(reinterpret_cast<unsigned char *>(d->result.data()));
    }

    return d->result;
}

QByteArray QnCryptographicHash::hash(const QByteArray &data, Algorithm algorithm) {
    QnCryptographicHash hash(algorithm);
    hash.addData(data);
    return hash.result();
}

}
}
