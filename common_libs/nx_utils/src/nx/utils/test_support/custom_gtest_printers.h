#pragma once

#include <chrono>
#include <iostream>

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QUrl>

NX_UTILS_API void PrintTo(const QByteArray& val, ::std::ostream* os);
NX_UTILS_API void PrintTo(const QString& val, ::std::ostream* os);
NX_UTILS_API void PrintTo(const QUrl& val, ::std::ostream* os);

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
