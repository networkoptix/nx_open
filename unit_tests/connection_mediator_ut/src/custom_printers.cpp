/**********************************************************
* 14 jan 2015
* a.kolesnikov
***********************************************************/

#include <iostream>
#include <string>

#include <QByteArray>
#include <QString>
#include <cdb/system_data.h>

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
