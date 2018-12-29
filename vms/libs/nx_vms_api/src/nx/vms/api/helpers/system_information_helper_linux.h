#pragma once

#include <QtCore/QString>

namespace nx::vms::api {

QString ubuntuVersionFromOsReleaseContents(const QByteArray& osReleaseContents);

} // namespace nx::vms::api
