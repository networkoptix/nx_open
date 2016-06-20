/**********************************************************
* 14 jan 2015
* a.kolesnikov
***********************************************************/

#include "custom_printers.h"

#include <iostream>
#include <string>

#include <QByteArray>
#include <QString>

#include <cloud_db_client/src/data/types.h>
#include <nx/fusion/serialization/lexical.h>


void PrintTo(const QByteArray& val, ::std::ostream* os) {
    *os << std::string(val.constData(), val.size());
}

void PrintTo(const QString& val, ::std::ostream* os) {
    *os << val.toStdString();
}

void PrintTo(const SocketAddress& val, ::std::ostream* os) {
    *os << val.toString().toStdString();
}

namespace nx {
namespace cdb {
namespace api {

void PrintTo(ResultCode val, ::std::ostream* os) {
    *os << QnLexical::serialized(val).toStdString();
}

}   //namespace api
}   //namespace cdb
}   //namespace nx


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

void PrintTo(const time_point<steady_clock>& val, ::std::ostream* os)
{
    *os << val.time_since_epoch().count() << "ns (utc)";
}

}   //chrono
}   //std
