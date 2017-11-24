#pragma once

#include <QtCore>

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_parser {

struct CustomizationInformation
{
    QString name;
    QList<QString> versions;
};

} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
