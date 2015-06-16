/**********************************************************
* 14 jan 2015
* a.kolesnikov
***********************************************************/

#include <iostream>
#include <string>

#include <QByteArray>


void printto(const qbytearray& val, ::std::ostream* os) {
    *os << std::string(val.constdata(), val.size());
}

