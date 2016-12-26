#include "custom_printers.h"

#include <iostream>
#include <string>

void PrintTo(const QByteArray& val, ::std::ostream* os)
{
    *os << std::string(val.constData(), val.size());
}

void PrintTo(const QString& val, ::std::ostream* os)
{
    *os << val.toStdString();
}

void PrintTo(const SocketAddress& val, ::std::ostream* os)
{
    *os << val.toString().toStdString();
}

void PrintTo(const QUrl& val, ::std::ostream* os)
{
    *os << val.toString().toStdString();
}
