/**********************************************************
* 14 jan 2015
* a.kolesnikov
***********************************************************/

#include "custom_printers.h"

#include <iostream>
#include <string>

#include <QByteArray>
#include <QString>

#include <cdb/system_data.h>
#include <utils/common/model_functions.h>


void PrintTo(const QByteArray& val, ::std::ostream* os) {
    *os << std::string(val.constData(), val.size());
}

void PrintTo(const QString& val, ::std::ostream* os) {
    *os << val.toStdString();
}

void PrintTo(const boost::optional<nx::cdb::api::SystemData>& val, ::std::ostream* os) {
    if( val )
        *os << "SystemData" << &val.get();
    else
        *os << "boost::none";
}

void PrintTo(const SocketAddress& val, ::std::ostream* os)
{
    *os << val.toString().toStdString();
}

void PrintTo(const nx::hpm::api::ResultCode& val, ::std::ostream* os)
{
    *os << QnLexical::serialized(val).toStdString();
}
