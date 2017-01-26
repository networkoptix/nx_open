/**********************************************************
* 14 jan 2015
* a.kolesnikov
***********************************************************/

#ifndef COMMON_PRINTERS_H
#define COMMON_PRINTERS_H

#include <chrono>
#include <iostream>

#include <QByteArray>
#include <QString>

#include <nx/network/socket_common.h>


//!Used by google test to print QByteArray as text
void PrintTo(const QByteArray& val, ::std::ostream* os);
void PrintTo(const QString& val, ::std::ostream* os);
void PrintTo(const SocketAddress& val, ::std::ostream* os);
void PrintTo(const HostAddress& val, ::std::ostream* os);

namespace std {
namespace chrono {
void PrintTo(const milliseconds& val, ::std::ostream* os);
void PrintTo(const seconds& val, ::std::ostream* os);
void PrintTo(const microseconds& val, ::std::ostream* os);
void PrintTo(const nanoseconds& val, ::std::ostream* os);
void PrintTo(const time_point<steady_clock>& val, ::std::ostream* os);
}   //chrono
}   //std

#endif  //COMMON_PRINTERS_H
