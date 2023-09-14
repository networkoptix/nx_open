// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <variant>

#include <QtCore/QByteArray>
#include <QtCore/QString>

class QIODevice;

namespace nx::vms::common {

class NX_VMS_COMMON_API FileSignature
{
public:
    enum class Result
    {
        ok,
        failed,
        ioError,
        internalError,
    };

    static QByteArray readKey(const QString& fileName);

    static std::optional<QByteArray> readSignatureFromZipFileComment(const QString& fileName);

    /** Sign the file using RSA private key. */
    static std::variant<QByteArray, Result> sign(
        const QString& fileName, const QByteArray& privateKey);

    /** Sign data using RSA private key. */
    static std::variant<QByteArray, Result> sign(
        QIODevice& inputDevice, const QByteArray& privateKey, qint64 size = -1);

    /** Sign ZIP file using RSA private key. */
    static Result signZipFile(const QString& fileName, const QByteArray& privateKey);

    /** Verify file signature using RSA public key. */
    static Result verify(
        const QString& fileName, const QByteArray& publicKey, const QByteArray& signature);

    /**
     * Verify data signature using RSA public key.
     * @param size Number of bytes to verify. If negative value is specified, verify all bytes.
     */
    static Result verify(
        QIODevice& inputDevice,
        const QByteArray& publicKey,
        const QByteArray& signature,
        qint64 size = -1);

    /** Verify ZIP file signature using RSA public key. */
    static Result verifyZipFile(
        const QString& fileName, const QByteArray& publicKey, QByteArray signature = {});
};

NX_VMS_COMMON_API QString toString(FileSignature::Result value);

} // namespace nx::vms::common
