/**********************************************************
* 14 jan 2015
* a.kolesnikov
***********************************************************/

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
