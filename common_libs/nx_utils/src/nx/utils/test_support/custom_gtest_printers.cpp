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

namespace nx {
namespace utils {
namespace db {

void PrintTo(const DBResult val, ::std::ostream* os)
{
    *os << toString(val);
}

} // namespace db
} // namespace utils
} // namespace nx
