// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx {
namespace utils {

void NX_UTILS_API createRandomFile(const QString& fileName, qint64 size);

} // namespace utils
} // namespace nx
