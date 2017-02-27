#include "custom_gtest_printers.h"

void PrintTo(const SocketAddress& val, ::std::ostream* os) {
    *os << val.toString().toStdString();
}
