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
#include <utils/serialization/lexical.h>


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

void PrintTo(api::ResultCode val, ::std::ostream* os) {
    *os << QnLexical::serialized(val).toStdString();
}

}   //cdb
}   //nx
