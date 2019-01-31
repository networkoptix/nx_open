#include "authutil.h"

#include <QCryptographicHash>

#include <nx/fusion/model_functions.h>
#include <nx/utils/string.h>

QMap<QByteArray, QByteArray> parseAuthData(const QByteArray& authData, char delimiter)
{
    QMap<QByteArray, QByteArray> result;

    const QList<QByteArray>& authParams = nx::utils::smartSplit(authData, delimiter);

    for (int i = 0; i < authParams.size(); ++i)
    {
        QByteArray data = authParams[i].trimmed();
        int pos = data.indexOf('=');
        if (pos == -1)
            return QMap<QByteArray, QByteArray>();

        QByteArray key = data.left(pos);
        QByteArray value = nx::utils::unquoteStr(data.mid(pos+1));

        result[key] = value;
    }

    return result;
}

QByteArray createUserPasswordDigest(
    const QString& userName,
    const QString& password,
    const QString& realm)
{
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(userName.toLower().toUtf8());
    md5.addData(":");
    md5.addData(realm.toUtf8());
    md5.addData(":");
    md5.addData(password.toUtf8());
    return md5.result().toHex();
}

QByteArray createHttpQueryAuthParam(
    const QString& userName,
    const QByteArray& digest,
    const QByteArray& method,
    QByteArray nonce)
{
    // Calculating HA2.
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(method);
    md5.addData(":");
    const QByteArray& partialHa2 = md5.result().toHex();

    // Calculating authentication digest.
    md5.reset();
    md5.addData(digest);
    md5.addData(":");
    md5.addData(nonce);
    md5.addData(":");
    md5.addData(partialHa2);
    const QByteArray& simplifiedHa2 = md5.result().toHex();

    return (userName.toUtf8().toLower() + ":" + nonce + ":" + simplifiedHa2).toBase64();
}

QByteArray createHttpQueryAuthParam(
    const QString& userName,
    const QString& password,
    const QString& realm,
    const QByteArray& method,
    QByteArray nonce)
{
    const auto& digest = createUserPasswordDigest(userName, password, realm);
    return createHttpQueryAuthParam(userName, digest, method, nonce);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((NonceReply), (json), _Fields)
