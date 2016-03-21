#include "authutil.h"
#include <QCryptographicHash>

template <class T, class T2>
QList<T> smartSplitInternal(const T& data, const T2 delimiter, const T2 quoteChar, bool keepEmptyParts)
{
    bool quoted = false;
    QList<T> rez;
    if (data.isEmpty())
        return rez;

    int lastPos = 0;
    for (int i = 0; i < data.size(); ++i)
    {
        if (data[i] == quoteChar)
            quoted = !quoted;
        else if (data[i] == delimiter && !quoted)
        {
            T value = data.mid(lastPos, i - lastPos);
            if (!value.isEmpty() || keepEmptyParts)
                rez << value;
            lastPos = i + 1;
        }
    }
    rez << data.mid(lastPos, data.size() - lastPos);

    return rez;
}

QList<QByteArray> smartSplit(const QByteArray& data, const char delimiter)
{
    return smartSplitInternal(data, delimiter, '\"', true);
}

QStringList smartSplit(const QString& data, const QChar delimiter, QString::SplitBehavior splitBehavior )
{
    return smartSplitInternal(data, delimiter, QChar(L'\"'), splitBehavior == QString::KeepEmptyParts);
}

template <class T, class T2>
T unquoteStrInternal(const T& v, T2 quoteChar)
{
    T value = v.trimmed();
    int pos1 = value.startsWith(quoteChar) ? 1 : 0;
    int pos2 = value.endsWith(quoteChar) ? 1 : 0;
    return value.mid(pos1, value.length()-pos1-pos2);
}

QByteArray unquoteStr(const QByteArray& v)
{
    return unquoteStrInternal(v, '\"');
}

QString unquoteStr(const QString& v)
{
    return unquoteStrInternal(v, L'\"');
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

