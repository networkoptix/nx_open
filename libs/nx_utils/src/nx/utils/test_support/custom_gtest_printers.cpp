// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "custom_gtest_printers.h"

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

void PrintTo(const QUrl& val, ::std::ostream* os)
{
    *os << val.toString().toStdString();
}

void PrintTo(const QSize& val, ::std::ostream* os)
{
    *os << "QSize(" << val.width() << " x " << val.height() << ")";
}

void PrintTo(const QSizeF& val, ::std::ostream* os)
{
    *os << "QSizeF(" << val.width() << " x " << val.height() << ")";
}

void PrintTo(const QRect& val, ::std::ostream* os)
{
    *os << "QRect(" << val.left() << ", " << val.top() << ", " << val.width() << " x "
        << val.height() << ")";
}

void PrintTo(const QRectF& val, ::std::ostream* os)
{
    *os << "QRectF(" << val.left() << ", " << val.top() << ", " << val.width() << " x "
        << val.height() << ")";
}

//-------------------------------------------------------------------------------------------------

namespace nx::utils::filesystem {

void PrintTo(const path& val, ::std::ostream* os)
{
    *os << val.string();
}

} // namespace nx::utils::filesystem
