#include "custom_printers.h"

#include <iostream>
#include <string>

#include <QByteArray>
#include <QString>

#include <nx/cloud/cdb/client/data/types.h>
#include <nx/fusion/serialization/lexical.h>

namespace nx {
namespace cdb {

void PrintTo(VmsResultCode val, ::std::ostream* os)
{
    *os << QnLexical::serialized(val).toStdString();
}

} // namespace cdb
} // namespace nx

namespace nx {
namespace utils {
namespace db {

void PrintTo(const DBResult val, ::std::ostream* os)
{
    *os << toString(val);
}

} // namespace db
} // namespace utils
} // namespace nx

namespace ec2 {

void PrintTo(ErrorCode val, ::std::ostream* os)
{
    *os << toString(val).toStdString();
}

} // namespace ec2
