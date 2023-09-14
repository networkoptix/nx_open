// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "file_signature.h"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <quazip/quazip.h>

#include <QtCore/QFile>
#include <QtCore/QRegularExpression>

#include <nx/utils/log/assert.h>
#include <nx/utils/type_utils.h>

namespace nx::vms::common {

namespace {

const QString kZipSignaturePrefix = "signature,digest,sha256,base64:";

qint64 getZipCommentStart(const QString& fileName)
{
    constexpr int kMaxCommentSize = std::numeric_limits<quint16>::max();
    const QByteArray kEocdPrefix = "\x50\x4b\x05\x06";
    constexpr int kEocdSize = 22;
    // We consider comment length bytes as a part of the comment.
    constexpr int kCommentPosInEocd = 20;

    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return -1;

    const auto size = file.size();

    if (size < kEocdSize)
        return -1;

    qint64 pos = size - kEocdSize;
    file.seek(pos);
    QByteArray buf = file.read(kEocdPrefix.size());
    while (pos >= 0 && size - pos < kMaxCommentSize + kEocdSize)
    {
        if (buf == kEocdPrefix)
            return pos + kCommentPosInEocd;

        for (int i = 3; i > 0; --i)
            buf[i] = buf[i - 1];

        --pos;
        file.seek(pos);
        file.read(buf.data(), 1);
    }

    return -1;
}

} // namespace

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

std::optional<QByteArray> FileSignature::readSignatureFromZipFileComment(const QString& fileName)
{
    QuaZip zip(fileName);
    if (!zip.open(QuaZip::mdUnzip))
        return std::nullopt;

    QRegularExpression regex(
        nx::format("^%1(.*)?(\n|\r|$)", kZipSignaturePrefix), QRegularExpression::MultilineOption);

    const QString comment = zip.getComment();
    const QRegularExpressionMatch match = regex.match(comment);
    if (!match.isValid())
        return QByteArray();

    return QByteArray::fromBase64(match.captured(1).toLatin1());
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
    QIODevice& inputDevice, const QByteArray& privateKey, qint64 size)
{
    if (privateKey.isEmpty())
        return FileSignature::Result::failed;

    BIO* bio = BIO_new_mem_buf(privateKey.constData(), privateKey.size());
    auto rsa = utils::wrapUnique(PEM_read_bio_RSAPrivateKey(bio, 0, 0, 0), &RSA_free);
    BIO_free(bio);

    if (!rsa)
        return FileSignature::Result::failed;

    auto ctx = utils::wrapUnique(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    auto pkey = utils::wrapUnique(EVP_PKEY_new(), &EVP_PKEY_free);
    EVP_PKEY_assign_RSA(pkey.get(), rsa.release());

    if (EVP_DigestSignInit(ctx.get(), nullptr, EVP_sha256(), nullptr, pkey.get()) <= 0)
        return Result::internalError;

    constexpr int kBufferSize = 1024 * 1024;
    QByteArray buffer(kBufferSize, Qt::Uninitialized);

    while (!inputDevice.atEnd() && size != 0)
    {
        qint64 bytesToRead = size >= 0 ? std::min<qint64>(kBufferSize, size) : kBufferSize;
        const qint64 bytesRead = inputDevice.read(buffer.data(), bytesToRead);
        if (bytesRead < 0)
            return Result::ioError;
        if (bytesRead == 0)
            break;

        if (EVP_DigestSignUpdate(ctx.get(), buffer, bytesRead) <= 0)
            return Result::internalError;

        if (size > 0)
            size -= bytesRead;
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

FileSignature::Result FileSignature::signZipFile(
    const QString& fileName, const QByteArray& privateKey)
{
    const qint64 commentPos = getZipCommentStart(fileName);
    if (commentPos == -1)
        return Result::ioError;

    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return Result::ioError;

    const auto result = sign(file, privateKey, commentPos);

    if (std::holds_alternative<Result>(result))
        return std::get<Result>(result);

    const QByteArray signature = std::get<QByteArray>(result);

    QuaZip zip(fileName);
    if (!zip.open(QuaZip::mdAdd))
        return Result::ioError;

    zip.setComment(kZipSignaturePrefix + signature.toBase64());

    return Result::ok;
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
    QIODevice& inputDevice, const QByteArray& publicKey, const QByteArray& signature, qint64 size)
{
    if (signature.isEmpty() || publicKey.isEmpty())
        return FileSignature::Result::failed;

    BIO* bio = BIO_new_mem_buf(publicKey.constData(), publicKey.size());
    auto rsa = utils::wrapUnique(PEM_read_bio_RSA_PUBKEY(bio, 0, 0, 0), &RSA_free);
    BIO_free(bio);

    if (!rsa)
        return FileSignature::Result::failed;

    auto ctx = utils::wrapUnique(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    auto pkey = utils::wrapUnique(EVP_PKEY_new(), &EVP_PKEY_free);
    EVP_PKEY_assign_RSA(pkey.get(), rsa.release());

    if (EVP_DigestVerifyInit(ctx.get(), nullptr, EVP_sha256(), nullptr, pkey.get()) <= 0)
        return Result::internalError;

    constexpr int kBufferSize = 1024 * 1024;
    QByteArray buffer(kBufferSize, Qt::Uninitialized);

    while (!inputDevice.atEnd() && size != 0)
    {
        qint64 bytesToRead = size >= 0 ? std::min<qint64>(kBufferSize, size) : kBufferSize;
        const qint64 bytesRead = inputDevice.read(buffer.data(), bytesToRead);
        if (bytesRead < 0)
            return Result::ioError;
        if (bytesRead == 0)
            break;

        if (EVP_DigestVerifyUpdate(ctx.get(), buffer, bytesRead) <= 0)
            return Result::internalError;

        if (size > 0)
            size -= bytesRead;
    }

    const int result = EVP_DigestVerifyFinal(
        ctx.get(), (const unsigned char*) signature.constData(), signature.size());

    return result == 1 ? Result::ok : Result::failed;
}

FileSignature::Result FileSignature::verifyZipFile(
    const QString& fileName, const QByteArray& publicKey, QByteArray signature)
{
    if (publicKey.isEmpty())
        return Result::failed;

    if (signature.isEmpty())
    {
        const auto result = readSignatureFromZipFileComment(fileName);
        if (!result)
            return Result::ioError;

        signature = *result;
    }

    if (signature.isEmpty())
        return Result::failed;

    const qint64 commentPos = getZipCommentStart(fileName);
    if (!NX_ASSERT(commentPos != -1))
        return Result::failed;

    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return Result::ioError;

    return verify(file, publicKey, signature, commentPos);
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
