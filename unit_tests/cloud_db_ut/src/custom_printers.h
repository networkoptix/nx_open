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

#include <cdb/result_code.h>
#include <nx_ec/impl/ec_api_impl.h>
#include <utils/db/types.h>


//!Used by google test to print QByteArray as text
void PrintTo(const QByteArray& val, ::std::ostream* os);
void PrintTo(const QString& val, ::std::ostream* os);
void PrintTo(const SocketAddress& val, ::std::ostream* os);

namespace nx {
namespace cdb {
namespace api {

void PrintTo(ResultCode val, ::std::ostream* os);

} // namespace api
} // namespace cdb

namespace db {

void PrintTo(const DBResult val, ::std::ostream* os);

} // namespace db

} // namespace nx

namespace std {
namespace chrono {

void PrintTo(const milliseconds& val, ::std::ostream* os);
void PrintTo(const seconds& val, ::std::ostream* os);
void PrintTo(const microseconds& val, ::std::ostream* os);
void PrintTo(const nanoseconds& val, ::std::ostream* os);
void PrintTo(const time_point<steady_clock>& val, ::std::ostream* os);

} // namespace chrono
} // namespace std

namespace ec2 {

void PrintTo(ErrorCode val, ::std::ostream* os);

} // namespace ec2

#endif  //COMMON_PRINTERS_H
