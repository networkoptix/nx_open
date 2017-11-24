#pragma once

#include <QtCore>

namespace nx {
namespace update {
namespace info {

struct FileInformation;

namespace detail {
namespace data_parser {

class AbstractTargetUpdatesInfoList
{
public:
    virtual ~AbstractTargetUpdatesInfoList() {}
    virtual QString targetName() = 0;
    virtual FileInformation* next() = 0;
};

} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
