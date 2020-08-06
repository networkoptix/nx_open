#pragma once

#include <variant>

#include <QtCore/QByteArray>
#include <QtCore/QString>

class QIODevice;

namespace nx::vms::common {

class FileSignature
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

    /** Sign the file using RSA private key. */
    static std::variant<QByteArray, Result> sign(
        const QString& fileName, const QByteArray& privateKey);

    /** Sign data using RSA private key. */
    static std::variant<QByteArray, Result> sign(
        QIODevice& inputDevice, const QByteArray& privateKey);

    /** Verify file signature using RSA public key. */
    static Result verify(
        const QString& fileName, const QByteArray& publicKey, const QByteArray& signature);

    /** Verify data signature using RSA public key. */
    static Result verify(
        QIODevice& inputDevice, const QByteArray& publicKey, const QByteArray& signature);
};

QString toString(FileSignature::Result value);

} // namespace nx::vms::common
