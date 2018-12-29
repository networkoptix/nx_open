#include "system_information.h"

#include <fstream>

#include <QtCore/QFile>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QTextStream>

#include "../helpers/system_information_helper_linux.h"

namespace nx::vms::api {

namespace {

static QByteArray osReleaseContents()
{
    QFile f("/etc/os-release");
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();

    return f.readAll();
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
    return ubuntuVersionFromOsReleaseContents(osReleaseContents());
}

} // namespace nx::vms::api
