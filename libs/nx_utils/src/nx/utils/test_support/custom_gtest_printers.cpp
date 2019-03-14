#include "custom_gtest_printers.h"

#include <iostream>
#include <string>

void PrintTo(const QByteArray& val, ::std::ostream* os)
{
    *os << std::string(val.constData(), val.size());
}

void PrintTo(const QString& val, ::std::ostream* os)
{
    *os << val.toStdString();
}

void PrintTo(const QUrl& val, ::std::ostream* os)
{
    *os << val.toString().toStdString();
}

void PrintTo(const QSize& val, ::std::ostream* os)
{
    *os << "QSize(" << val.width() << "x" << val.height() << ")";
}

void PrintTo(const QSizeF& val, ::std::ostream* os)
{
    *os << "QSizeF(" << val.width() << "x" << val.height() << ")";
}

//-------------------------------------------------------------------------------------------------

namespace std {
namespace chrono {

void PrintTo(const milliseconds& val, ::std::ostream* os)
{
    *os << val.count() << "ms";
}

void PrintTo(const seconds& val, ::std::ostream* os)
{
    *os << val.count() << "s";
}

void PrintTo(const microseconds& val, ::std::ostream* os)
{
    *os << val.count() << "usec";
}

void PrintTo(const nanoseconds& val, ::std::ostream* os)
{
    *os << val.count() << "nanosec";
}

void PrintTo(const steady_clock::time_point& val, ::std::ostream* os)
{
    *os << val.time_since_epoch().count() << "ns (monotonic)";
}

void PrintTo(const system_clock::time_point& val, ::std::ostream* os)
{
    *os << val.time_since_epoch().count() << "ns (utc)";
}

} // namespace chrono
} // namespace std

//-------------------------------------------------------------------------------------------------

namespace std {
namespace filesystem {

void PrintTo(const path& val, ::std::ostream* os)
{
    *os << val.string();
}

} // namespace filesystem
} // namespace std

//-------------------------------------------------------------------------------------------------

namespace nx {
namespace utils {

void PrintTo(const Url& val, ::std::ostream* os)
{
    *os << val.toString().toStdString();
}

} // namespace utils
} // namespace nx
