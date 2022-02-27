// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <iostream>
#include <string_view>

#include <QtCore/QByteArray>
#include <QtCore/QRect>
#include <QtCore/QString>
#include <QtCore/QSize>
#include <QtCore/QUrl>

#include "../std/filesystem.h"
#include "../url.h"

NX_UTILS_API void PrintTo(const QByteArray& val, ::std::ostream* os);
NX_UTILS_API void PrintTo(const QString& val, ::std::ostream* os);
NX_UTILS_API void PrintTo(const QUrl& val, ::std::ostream* os);
NX_UTILS_API void PrintTo(const QSize& val, ::std::ostream* os);
NX_UTILS_API void PrintTo(const QSizeF& val, ::std::ostream* os);
NX_UTILS_API void PrintTo(const QRect& val, ::std::ostream* os);
NX_UTILS_API void PrintTo(const QRectF& val, ::std::ostream* os);

//-------------------------------------------------------------------------------------------------

namespace nx::utils::filesystem {

NX_UTILS_API void PrintTo(const path& val, ::std::ostream* os);

} // namespace nx::utils::filesystem
