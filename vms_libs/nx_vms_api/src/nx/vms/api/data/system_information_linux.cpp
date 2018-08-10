#include "system_information.h"

#include <fstream>

#include <QtCore/QFile>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QTextStream>

namespace nx::vms::api {

namespace {

static QMap<QString, QString> osReleaseContents()
{
    QFile f("/etc/os-release");
    if (!f.open(QIODevice::ReadOnly))
        return QMap<QString, QString>();

    QTextStream stream(f.readAll());
    QMap<QString, QString> result;
    while (!stream.atEnd())
    {
        QString line = stream.readLine();
        if (line.isEmpty())
            continue;

        QRegExp keyValueRegExp("^(.+)=(.+)$");
        int reIndex = keyValueRegExp.indexIn(line);
        if (reIndex == -1 || keyValueRegExp.captureCount() != 2)
            continue;

        auto captureList = keyValueRegExp.capturedTexts();
        result.insert(captureList[1], captureList[2].replace("\"", ""));
    }

    return result;
}

} // namespace

QString SystemInformation::currentSystemRuntime()
{
    const auto contents = osReleaseContents();
    const QString kPrettyNameKey = "PRETTY_NAME";

    return contents.contains(kPrettyNameKey)
        ? contents[kPrettyNameKey] : "GNU/Linux without /etc/os-release";
}

QString SystemInformation::runtimeOsVersion()
{
    const auto contents = osReleaseContents();
    if (contents.isEmpty())
        return QString();

    if (contents.contains("ID"))
    {
        const auto idValue = contents["ID"].toLower();
        if (idValue == "ubuntu")
        {
            QRegExp versionRegExp("[^0-9]*([0-9]+\\.[0-9+]+\\.[0-9]+)[^0-9]*");
            int reIndex = versionRegExp.indexIn(idValue);
            if (reIndex == -1 || versionRegExp.captureCount() != 1)
                return QString();

            return versionRegExp.capturedTexts()[1];
        }

        if (contents.contains("ID_LIKE"))
        {
            const auto idLikeValue = contents["ID_LIKE"].toLower();
            if (idLikeValue != "ubuntu")
                return QString();

            if (!contents.contains("UBUNTU_CODENAME"))
                return QString();

            const auto ubuntuCodename = contents["UBUNTU_CODENAME"].toLower();
            if (ubuntuCodename.contains("trusty"))
                return "14.04";
            if (ubuntuCodename.contains("xenial"))
                return "16.04";
            if (ubuntuCodename.contains("bionic"))
                return "18.04";
            if (ubuntuCodename.contains("tahr"))
                return "14.04";
            if (ubuntuCodename.contains("precise"))
                return "12.04";
            if (ubuntuCodename.contains("quantal"))
                return "12.10";
            if (ubuntuCodename.contains("raring"))
                return "13.04";
            if (ubuntuCodename.contains("saucy"))
                return "13.10";
            if (ubuntuCodename.contains("utopic"))
                return "14.10";
            if (ubuntuCodename.contains("vivid"))
                return "15.04";
            if (ubuntuCodename.contains("willy"))
                return "15.10";
            if (ubuntuCodename.contains("yakkety"))
                return "16.10";
            if (ubuntuCodename.contains("zesty"))
                return "17.04";
            if (ubuntuCodename.contains("artful"))
                return "17.10";
        }
    }

    return QString();
}

} // namespace nx::vms::api