#pragma once

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_parser {

class AbstractCustomizationInfo;

class AbstractCustomizationInfoList
{
public:
    virtual ~AbstractCustomizationInfoList() {}
    virtual AbstractCustomizationInfo* next() = 0;
};

} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
