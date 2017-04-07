#include "authutil.h"
#include <QCryptographicHash>

#include <nx/fusion/model_functions.h>
#include <nx/utils/string.h>

QMap<QByteArray, QByteArray> parseAuthData(const QByteArray &authData, char delimiter) {
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
    const QString& realm )
{
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(QString(lit("%1:%2:%3")).arg(userName.toLower(), realm, password).toLatin1());
    return md5.result().toHex();
}

QByteArray createHttpQueryAuthParam(
    const QString& userName,
    const QString& password,
    const QString& realm,
    const QByteArray& method,
    QByteArray nonce)
{
    return createHttpQueryAuthParam(userName, createUserPasswordDigest(userName, password, realm), method, nonce);
}

QByteArray createHttpQueryAuthParam(
    const QString& userName,
    const QByteArray& digest,
    const QByteArray& method,
    QByteArray nonce)
{
    //calculating "HA2"
    QCryptographicHash md5Hash( QCryptographicHash::Md5 );
    md5Hash.addData( method );
    md5Hash.addData( ":" );
    const QByteArray nedoHa2 = md5Hash.result().toHex();

    //calculating auth digest
    md5Hash.reset();
    md5Hash.addData( digest );
    md5Hash.addData( ":" );
    md5Hash.addData( nonce );
    md5Hash.addData( ":" );
    md5Hash.addData( nedoHa2 );
    const QByteArray& authDigest = md5Hash.result().toHex();

    return (userName.toUtf8().toLower() + ":" + nonce + ":" + authDigest).toBase64();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((NonceReply), (json), _Fields)
