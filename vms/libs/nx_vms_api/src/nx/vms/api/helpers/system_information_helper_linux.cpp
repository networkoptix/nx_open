#include "system_information_helper_linux.h"

#include <QtCore/QTextStream>

namespace nx::vms::api {

QString ubuntuVersionFromCodeName(const QString& codeName)
{
    if (codeName.isEmpty())
        return QString();

    if (codeName.contains("trusty"))
        return "14.04";
    if (codeName.contains("xenial"))
        return "16.04";
    if (codeName.contains("bionic"))
        return "18.04";
    if (codeName.contains("tahr"))
        return "14.04";
    if (codeName.contains("precise"))
        return "12.04";
    if (codeName.contains("quantal"))
        return "12.10";
    if (codeName.contains("raring"))
        return "13.04";
    if (codeName.contains("saucy"))
        return "13.10";
    if (codeName.contains("utopic"))
        return "14.10";
    if (codeName.contains("vivid"))
        return "15.04";
    if (codeName.contains("willy"))
        return "15.10";
    if (codeName.contains("yakkety"))
        return "16.10";
    if (codeName.contains("zesty"))
        return "17.04";
    if (codeName.contains("artful"))
        return "17.10";

    return QString();
}

QString osReleaseContentsValueByKey(const QByteArray& osReleaseContents, const QString& key)
{
    if (osReleaseContents.isEmpty())
        return QString();

    QTextStream stream(osReleaseContents);
    while (!stream.atEnd())
    {
        QString line = stream.readLine();
        if (line.isEmpty())
            continue;

        if (line.toLower().contains(key.toLower()))
        {
            int equalSignPos = line.indexOf('=');
            if (equalSignPos == -1 || line.size() == equalSignPos + 1)
                return QString();

            return line.mid(equalSignPos + 1);
        }
    }

    return QString();
}

} // namespace nx::vms::api
