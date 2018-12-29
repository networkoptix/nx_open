#include "system_information_helper_linux.h"

#include <QtCore/QTextStream>

namespace nx::vms::api {

QString ubuntuVersionFromOsReleaseContents(const QByteArray& osReleaseContents)
{
    if (osReleaseContents.isEmpty())
        return QString();

    QTextStream stream(osReleaseContents);
    while (!stream.atEnd())
    {
        QString line = stream.readLine().toLower();
        if (line.isEmpty())
            continue;

        if (line.contains("ubuntu_codename"))
        {
            int equalSignPos = line.indexOf('=');
            if (equalSignPos == -1 || line.size() == equalSignPos + 1)
                return QString();

            const QString codename = line.mid(equalSignPos + 1).toLower();

            if (codename.contains("trusty"))
                return "14.04";
            if (codename.contains("xenial"))
                return "16.04";
            if (codename.contains("bionic"))
                return "18.04";
            if (codename.contains("tahr"))
                return "14.04";
            if (codename.contains("precise"))
                return "12.04";
            if (codename.contains("quantal"))
                return "12.10";
            if (codename.contains("raring"))
                return "13.04";
            if (codename.contains("saucy"))
                return "13.10";
            if (codename.contains("utopic"))
                return "14.10";
            if (codename.contains("vivid"))
                return "15.04";
            if (codename.contains("willy"))
                return "15.10";
            if (codename.contains("yakkety"))
                return "16.10";
            if (codename.contains("zesty"))
                return "17.04";
            if (codename.contains("artful"))
                return "17.10";
        }
    }

    return QString();
}

} // namespace nx::vms::api
