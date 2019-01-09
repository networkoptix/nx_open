#include "system_information.h"

#include <fstream>

#include <QtCore/QFile>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QTextStream>

#include <nx/vms/api/helpers/system_information_helper_linux.h>

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
    const auto prettyName = osReleaseContentsValueByKey(osReleaseContents(), "pretty_name");
    return prettyName.isEmpty() ? "GNU/Linux without /etc/os-release" : prettyName;
}

QString SystemInformation::runtimeOsVersion()
{
    return ubuntuVersionFromCodeName(
        osReleaseContentsValueByKey(osReleaseContents(), "ubuntu_codename"));
}

} // namespace nx::vms::api
