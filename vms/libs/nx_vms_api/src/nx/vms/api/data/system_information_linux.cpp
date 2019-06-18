#include "system_information.h"

#include <fstream>

#include <QtCore/QFile>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QTextStream>

#include <nx/utils/platform/ubuntu_version_linux.h>

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
    const auto prettyName = utils::osReleaseContentsValueByKey(osReleaseContents(), "pretty_name");
    return prettyName.isEmpty() ? "GNU/Linux without /etc/os-release" : prettyName;
}

QString SystemInformation::runtimeOsVersion()
{
    const auto codenameVersion = utils::ubuntuVersionFromCodeName(
        utils::osReleaseContentsValueByKey(osReleaseContents(), "ubuntu_codename"));

    if (!codenameVersion.isEmpty())
        return codenameVersion;

    return utils::osReleaseContentsValueByKey(osReleaseContents(), "version_id");
}

} // namespace nx::vms::api
