/**********************************************************
* 14 jan 2015
* a.kolesnikov
***********************************************************/

#ifndef COMMON_PRINTERS_H
#define COMMON_PRINTERS_H

#include <iostream>

#include <QByteArray>
#include <QString>

#include <cdb/result_code.h>
#include <nx/network/socket_common.h>


//!Used by google test to print QByteArray as text
void PrintTo(const QByteArray& val, ::std::ostream* os);
void PrintTo(const QString& val, ::std::ostream* os);
void PrintTo(const SocketAddress& val, ::std::ostream* os);

namespace nx {
namespace cdb {

void PrintTo(api::ResultCode val, ::std::ostream* os);

}   //cdb
}   //nx

#endif  //COMMON_PRINTERS_H
