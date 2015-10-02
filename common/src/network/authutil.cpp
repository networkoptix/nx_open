#include "authutil.h"

template <class T, class T2>
QList<T> smartSplitInternal(const T& data, const T2 delimiter, const T2 quoteChar)
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
            rez << data.mid(lastPos, i - lastPos);
            lastPos = i + 1;
        }
    }
    rez << data.mid(lastPos, data.size() - lastPos);

    return rez;
}

QList<QByteArray> smartSplit(const QByteArray& data, const char delimiter)
{
    return smartSplitInternal(data, delimiter, '\"');
}

QStringList smartSplit(const QString& data, const QChar delimiter)
{
    return smartSplitInternal(data, delimiter, QChar(L'\"'));
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

