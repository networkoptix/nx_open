#include "custom_printers.h"

#include <nx/fusion/model_functions.h>

void PrintTo(const Qn::GlobalPermissions& val, ::std::ostream* os)
{
    *os << QnLexical::serialized(val).toStdString();
}

void PrintTo(const Qn::Permissions& val, ::std::ostream* os)
{
    *os << QnLexical::serialized(val).toStdString();
}

void PrintTo(const QByteArray& val, ::std::ostream* os)
{
    *os << std::string(val.constData(), val.size());
}

void PrintTo(const QString& val, ::std::ostream* os)
{
    *os << val.toStdString();
}
