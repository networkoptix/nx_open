#pragma once

#include <QtCore/QString>

namespace nx::utils {

QString ubuntuVersionFromCodeName(const QString& codeName);
QString osReleaseContentsValueByKey(const QByteArray& osReleaseContents, const QString& key);

} // namespace nx::utils
