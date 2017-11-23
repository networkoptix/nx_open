#pragma once

namespace nx {
namespace update {
namespace info {
namespace detail {

class AbstractCustomizationInfo;

class AbstractCustomizationInfoList
{
public:
    virtual AbstractCustomizationInfo* next() = 0;
};

} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
