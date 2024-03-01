// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_CRYPTOGRAPHIC_HASH_H
#define QN_CRYPTOGRAPHIC_HASH_H

#include <string_view>

#include <QtCore/QCryptographicHash>
#include <QtCore/QScopedPointer>

#include "buffer.h"

class QIODevice;

namespace nx::utils {

class QnCryptographicHashPrivate;

/**
 * Just like <tt>QCryptographicHash</tt>, but works MUCH faster.
 * Uses OpenSSL internally.
 */
class NX_UTILS_API QnCryptographicHash
{
public:
    enum Algorithm {
        Md4 = QCryptographicHash::Md4,
        Md5 = QCryptographicHash::Md5,
        Sha1 = QCryptographicHash::Sha1,
        Sha256 = QCryptographicHash::Sha256,
        Sha3_256 = QCryptographicHash::Sha3_256,
        Sha3_512 = QCryptographicHash::Sha3_512,
    };

    QnCryptographicHash(Algorithm algorithm);
    ~QnCryptographicHash();

    QnCryptographicHash(const QnCryptographicHash &other);
    QnCryptographicHash &operator=(const QnCryptographicHash &other);

    void addData(const char *data, int length);
    void addData(const QByteArray& data);
    void addData(const nx::Buffer& data);
    void addData(const std::string_view& data);
    void addData(const std::string& data);
    void addData(const char* data);
    bool addData(QIODevice* device);

    void reset();

    QByteArray result() const;

    static QByteArray hash(const QByteArray &data, Algorithm algorithm);

private:
    QScopedPointer<QnCryptographicHashPrivate> d;
};

inline QByteArray md5(const QByteArray& data)
{
    return QnCryptographicHash::hash(data, QnCryptographicHash::Md5);
}

inline QByteArray sha1(const QByteArray& data)
{
    return QnCryptographicHash::hash(data, QnCryptographicHash::Sha1);
}

NX_UTILS_API std::string sha1(const std::string_view& data);

NX_UTILS_API QByteArray sha3_256(const QByteArray& data);
NX_UTILS_API std::string sha3_256(const std::string_view& data);

NX_UTILS_API QByteArray sha3_512(const QByteArray& data);
NX_UTILS_API std::string sha3_512(const std::string_view& data);

} // namespace nx::utils

#endif // QN_CRYPTOGRAPHIC_HASH_H
