
#include "StdAfx.h"

#include <string>


void PrintTo(const QByteArray& val, ::std::ostream* os) {
    *os << std::string(val.constData(), val.size());
}
