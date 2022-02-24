// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::utils {

QString NX_UTILS_API ubuntuVersionFromCodeName(const QString& codeName);
QString NX_UTILS_API osReleaseContentsValueByKey(const QByteArray& osReleaseContents, const QString& key);

} // namespace nx::utils
