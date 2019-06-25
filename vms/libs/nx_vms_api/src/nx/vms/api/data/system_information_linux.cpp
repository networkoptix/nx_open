#include "system_information.h"

#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QRegularExpression>

namespace nx::vms::api {

namespace {

static QByteArray osReleaseContents()
{
    QFile f("/etc/os-release");
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();

    return f.readAll();
}

static QString unquoteIfNeeded(const QString& s)
{
    if (s.size() < 2)
        return s;

    if (s.startsWith(L'"') && s.endsWith(L'"'))
        return s.mid(1, s.size() - 2);

    return s;
}

QString osReleaseContentsValueByKey(
    const QByteArray& osReleaseContents, const QString& key, const QString& defaultValue = {})
{
    if (osReleaseContents.isEmpty())
        return defaultValue;

    const QRegularExpression re(
        QString("^%1\\s*=\\s*(.+)$").arg(key), QRegularExpression::CaseInsensitiveOption);

    QTextStream stream(osReleaseContents);
    while (!stream.atEnd())
    {
        const QString line = stream.readLine().trimmed();
        const QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch())
            return unquoteIfNeeded(match.captured(1));
    }

    return defaultValue;
}

} // namespace

QString SystemInformation::currentSystemRuntime()
{
    return osReleaseContentsValueByKey(
        osReleaseContents(), "PRETTY_NAME", "GNU/Linux without /etc/os-release");
}

QString SystemInformation::runtimeModification()
{
    if (!runtimeModificationOverride.isEmpty())
        return runtimeModificationOverride;
    return osReleaseContentsValueByKey(osReleaseContents(), "ID");
}

QString SystemInformation::runtimeOsVersion()
{
    if (!runtimeOsVersionOverride.isEmpty())
        return runtimeOsVersionOverride;
    return osReleaseContentsValueByKey(osReleaseContents(), "VERSION_ID");
}

} // namespace nx::vms::api
