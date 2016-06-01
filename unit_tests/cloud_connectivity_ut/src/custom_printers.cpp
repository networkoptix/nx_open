/**********************************************************
* 14 jan 2015
* a.kolesnikov
***********************************************************/

#include "custom_printers.h"

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

void PrintTo(const SocketAddress& val, ::std::ostream* os) {
    *os << val.toString().toStdString();
}

void PrintTo(const HostAddress& val, ::std::ostream* os) {
    *os << val.toString().toStdString();
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

}   //chrono
}   //std
