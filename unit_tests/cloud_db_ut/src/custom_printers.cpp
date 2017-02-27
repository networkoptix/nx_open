#include "custom_printers.h"

#include <iostream>
#include <string>

#include <QByteArray>
#include <QString>

#include <cloud_db_client/src/data/types.h>
#include <nx/fusion/serialization/lexical.h>

namespace nx {
namespace cdb {
namespace api {

void PrintTo(ResultCode val, ::std::ostream* os)
{
    *os << QnLexical::serialized(val).toStdString();
}

} // namespace api
} // namespace cdb

namespace db {

void PrintTo(const DBResult val, ::std::ostream* os)
{
    *os << QnLexical::serialized(val).toStdString();
}

} // namespace db

} // namespace nx


namespace ec2 {

void PrintTo(ErrorCode val, ::std::ostream* os)
{
    *os << toString(val).toStdString();
}

} // namespace ec2
