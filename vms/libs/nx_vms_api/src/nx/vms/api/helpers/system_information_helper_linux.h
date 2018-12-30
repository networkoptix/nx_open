#pragma once

#include <QtCore/QString>

namespace nx::vms::api {

QString ubuntuVersionFromCodeName(const QString& codeName);
QString osReleaseContentsValueByKey(const QByteArray& osReleaseContents, const QString& key);

} // namespace nx::vms::api
