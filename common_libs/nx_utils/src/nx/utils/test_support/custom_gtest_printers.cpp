#include "custom_gtest_printers.h"

#include <iostream>
#include <string>

#include <QByteArray>
#include <QString>

void PrintTo(const QByteArray& val, ::std::ostream* os) {
    *os << std::string(val.constData(), val.size());
}

void PrintTo(const QString& val, ::std::ostream* os) {
    *os << val.toStdString();
}

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
    *os << duration_cast<milliseconds>(val.time_since_epoch()).count() << "ms (utc)";
}

} // namespace chrono
} // namespace std
