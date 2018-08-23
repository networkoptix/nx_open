#pragma once

#include <chrono>
#include <iostream>

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QUrl>

#include "../std/filesystem.h"
#include "../url.h"

NX_UTILS_API void PrintTo(const QByteArray& val, ::std::ostream* os);
NX_UTILS_API void PrintTo(const QString& val, ::std::ostream* os);
NX_UTILS_API void PrintTo(const QUrl& val, ::std::ostream* os);

//-------------------------------------------------------------------------------------------------

namespace std {
namespace chrono {

NX_UTILS_API void PrintTo(const milliseconds& val, ::std::ostream* os);
NX_UTILS_API void PrintTo(const seconds& val, ::std::ostream* os);
NX_UTILS_API void PrintTo(const microseconds& val, ::std::ostream* os);
NX_UTILS_API void PrintTo(const nanoseconds& val, ::std::ostream* os);
NX_UTILS_API void PrintTo(const steady_clock::time_point& val, ::std::ostream* os);
NX_UTILS_API void PrintTo(const system_clock::time_point& val, ::std::ostream* os);

} // namespace chrono
} // namespace std

//-------------------------------------------------------------------------------------------------

namespace std {
namespace filesystem {

NX_UTILS_API void PrintTo(const path& val, ::std::ostream* os);

} // namespace filesystem
} // namespace std

//-------------------------------------------------------------------------------------------------

namespace nx {
namespace utils {

NX_UTILS_API void PrintTo(const Url& val, ::std::ostream* os);

} // namespace utils
} // namespace nx
