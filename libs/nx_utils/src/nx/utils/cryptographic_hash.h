#ifndef QN_CRYPTOGRAPHIC_HASH_H
#define QN_CRYPTOGRAPHIC_HASH_H

#include <QtCore/QCryptographicHash>
#include <QtCore/QScopedPointer>

class QIODevice;

namespace nx {
namespace utils {

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
        Sha1 = QCryptographicHash::Sha1
    };

    QnCryptographicHash(Algorithm algorithm);
    ~QnCryptographicHash();

    QnCryptographicHash(const QnCryptographicHash &other);
    QnCryptographicHash &operator=(const QnCryptographicHash &other);

    void addData(const char *data, int length);
    void addData(const QByteArray &data);
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

}
}

#endif // QN_CRYPTOGRAPHIC_HASH_H
