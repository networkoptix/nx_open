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

#include <nx/utils/log/to_string.h>

//!Used by google test to print QByteArray as text
void PrintTo(const QByteArray& val, ::std::ostream* os);
void PrintTo(const QString& val, ::std::ostream* os);

namespace std {
namespace chrono {

template<typename Rep, typename Period>
void PrintTo(const duration<Rep, Period>& val, ::std::ostream* os)
{
    *os << toString(val).toStdString();
}

void PrintTo(const time_point<steady_clock>& val, ::std::ostream* os);

} // namespace chrono
} // namespace std

#endif  //COMMON_PRINTERS_H
