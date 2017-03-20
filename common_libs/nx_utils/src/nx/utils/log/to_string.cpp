#include "to_string.h"

QString toString(const char* s)
{
    return QString::fromUtf8(s);
}

QString toString(void* p)
{
    return QString::fromLatin1("0x%1").arg(reinterpret_cast<qulonglong>(p), 0, 16);
}

QString toString(const QByteArray& t)
{
    return QString::fromUtf8(t);
}

QString toString(const QUrl& url)
{
    return url.toString(QUrl::RemovePassword);
}

QString toString(const std::string& t)
{
    return QString::fromStdString(t);
}

QString toString(const std::chrono::hours& t)
{
    return QString(QLatin1String("%1h")).arg(t.count());
}

QString toString(const std::chrono::minutes& t)
{
    if (t.count() % 60 == 0)
        return toString(std::chrono::duration_cast<std::chrono::hours>(t));

    return QString(QLatin1String("%1m")).arg(t.count());
}

QString toString(const std::chrono::seconds& t)
{
    if (t.count() % 1000 == 0)
        return toString(std::chrono::duration_cast<std::chrono::minutes>(t));

    return QString(QLatin1String("%1s")).arg(t.count());
}

QString toString(const std::chrono::milliseconds& t)
{
    if (t.count() % 1000 == 0)
        return toString(std::chrono::duration_cast<std::chrono::seconds>(t));

    return QString(QLatin1String("%1ms")).arg(t.count());
}
