#include "file_signature.h"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <QtCore/QFile>

#include <nx/utils/type_utils.h>

namespace nx::vms::common {

QByteArray FileSignature::readKey(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return {};

    constexpr int kMaxFileSize = 1024 * 1024;
    if (file.size() > kMaxFileSize)
        return {};

    return file.readAll();
}

std::variant<QByteArray, FileSignature::Result> FileSignature::sign(
    const QString& fileName, const QByteArray& privateKey)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return Result::ioError;

    return sign(file, privateKey);
}

std::variant<QByteArray, FileSignature::Result> FileSignature::sign(
    QIODevice& inputDevice, const QByteArray& privateKey)
{
    if (privateKey.isEmpty())
        return FileSignature::Result::failed;

    BIO* bio = BIO_new_mem_buf(privateKey.constData(), privateKey.size());
    auto rsa = utils::wrapUnique(PEM_read_bio_RSAPrivateKey(bio, 0, 0, 0), &RSA_free);
    BIO_free(bio);

    if (!rsa)
        return FileSignature::Result::failed;

    auto ctx = utils::wrapUnique(EVP_MD_CTX_create(), &EVP_MD_CTX_destroy);
    auto pkey = utils::wrapUnique(EVP_PKEY_new(), &EVP_PKEY_free);
    EVP_PKEY_assign_RSA(pkey.get(), rsa.release());

    if (EVP_DigestSignInit(ctx.get(), nullptr, EVP_sha256(), nullptr, pkey.get()) <= 0)
        return Result::internalError;

    constexpr int kBufferSize = 1024 * 1024;
    QByteArray buffer(kBufferSize, Qt::Uninitialized);

    while (!inputDevice.atEnd())
    {
        const qint64 bytesRead = inputDevice.read(buffer.data(), kBufferSize);
        if (bytesRead < 0)
            return Result::ioError;
        if (bytesRead == 0)
            break;

        if (EVP_DigestSignUpdate(ctx.get(), buffer, bytesRead) <= 0)
            return Result::internalError;
    }

    size_t signatureSize = 0;
    int result = EVP_DigestSignFinal(ctx.get(), nullptr, &signatureSize);
    if (result != 1)
        return Result::internalError;

    QByteArray signature(signatureSize, Qt::Uninitialized);
    result = EVP_DigestSignFinal(ctx.get(), (unsigned char*) signature.data(), &signatureSize);
    if (result != 1)
        return Result::internalError;

    return signature;
}

FileSignature::Result FileSignature::verify(
    const QString& fileName, const QByteArray& publicKey, const QByteArray& signature)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return Result::ioError;

    return verify(file, publicKey, signature);
}

FileSignature::Result FileSignature::verify(
    QIODevice& inputDevice, const QByteArray& publicKey, const QByteArray& signature)
{
    if (signature.isEmpty() || publicKey.isEmpty())
        return FileSignature::Result::failed;

    BIO* bio = BIO_new_mem_buf(publicKey.constData(), publicKey.size());
    auto rsa = utils::wrapUnique(PEM_read_bio_RSA_PUBKEY(bio, 0, 0, 0), &RSA_free);
    BIO_free(bio);

    if (!rsa)
        return FileSignature::Result::failed;

    auto ctx = utils::wrapUnique(EVP_MD_CTX_create(), &EVP_MD_CTX_destroy);
    auto pkey = utils::wrapUnique(EVP_PKEY_new(), &EVP_PKEY_free);
    EVP_PKEY_assign_RSA(pkey.get(), rsa.release());

    if (EVP_DigestVerifyInit(ctx.get(), nullptr, EVP_sha256(), nullptr, pkey.get()) <= 0)
        return Result::internalError;

    constexpr int kBufferSize = 1024 * 1024;
    QByteArray buffer(kBufferSize, Qt::Uninitialized);

    while (!inputDevice.atEnd())
    {
        const qint64 bytesRead = inputDevice.read(buffer.data(), kBufferSize);
        if (bytesRead < 0)
            return Result::ioError;
        if (bytesRead == 0)
            break;

        if (EVP_DigestVerifyUpdate(ctx.get(), buffer, bytesRead) <= 0)
            return Result::internalError;
    }

    const int result = EVP_DigestVerifyFinal(
        ctx.get(), (const unsigned char*) signature.constData(), signature.size());

    return result == 1 ? Result::ok : Result::failed;
}

QString toString(FileSignature::Result value)
{
    switch (value)
    {
        case FileSignature::Result::ok:
            return "ok";
        case FileSignature::Result::failed:
            return "failed";
        case FileSignature::Result::ioError:
            return "ioError";
        case FileSignature::Result::internalError:
            return "internalError";
    }
    return {};
}

} // namespace nx::vms::common
