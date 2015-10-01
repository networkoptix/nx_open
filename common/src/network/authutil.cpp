#include "authutil.h"
#include <QCryptographicHash>

//!Splits \a data by \a delimiter not closed within quotes
/*!
    E.g.: 
    \code
    one, two, "three, four"
    \endcode

    will be splitted to 
    \code
    one
    two
    "three, four"
    \endcode
*/
QList<QByteArray> smartSplit(const QByteArray& data, const char delimiter)
{
    bool quoted = false;
    QList<QByteArray> rez;
    if (data.isEmpty())
        return rez;

    int lastPos = 0;
    for (int i = 0; i < data.size(); ++i)
    {
        if (data[i] == '\"')
            quoted = !quoted;
        else if (data[i] == delimiter && !quoted)
        {
            rez << QByteArray(data.constData() + lastPos, i - lastPos);
            lastPos = i + 1;
        }
    }
    rez << QByteArray(data.constData() + lastPos, data.size() - lastPos);

    return rez;
}

QByteArray unquoteStr(const QByteArray& v)
{
    QByteArray value = v.trimmed();
    int pos1 = value.startsWith('\"') ? 1 : 0;
    int pos2 = value.endsWith('\"') ? 1 : 0;
    return value.mid(pos1, value.length()-pos1-pos2);
}

QMap<QByteArray, QByteArray> parseAuthData(const QByteArray &authData, char delimiter) {
    QMap<QByteArray, QByteArray> result;

    const QList<QByteArray>& authParams = smartSplit(authData, delimiter);

    for (int i = 0; i < authParams.size(); ++i)
    {
        QByteArray data = authParams[i].trimmed();
        int pos = data.indexOf('=');
        if (pos == -1)
            return QMap<QByteArray, QByteArray>();

        QByteArray key = data.left(pos);
        QByteArray value = unquoteStr(data.mid(pos+1));

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
    md5.addData(QString(lit("%1:%2:%3")).arg(userName, realm, password).toLatin1());
    return md5.result().toHex();
}

QByteArray createHttpQueryAuthParam(
    const QString& userName,
    const QString& password,
    const QString& realm,
    const QByteArray& method,
    QByteArray nonce)
{
    //calculating user digest
    const QByteArray& ha1 = createUserPasswordDigest( userName, password, realm );

    //calculating "HA2"
    QCryptographicHash md5Hash( QCryptographicHash::Md5 );
    md5Hash.addData( method );
    md5Hash.addData( ":" );
    const QByteArray nedoHa2 = md5Hash.result().toHex();

    //calculating auth digest
    md5Hash.reset();
    md5Hash.addData( ha1 );
    md5Hash.addData( ":" );
    md5Hash.addData( nonce );
    md5Hash.addData( ":" );
    md5Hash.addData( nedoHa2 );
    const QByteArray& authDigest = md5Hash.result().toHex();

    return (userName.toUtf8() + ":" + nonce + ":" + authDigest).toBase64();
}

