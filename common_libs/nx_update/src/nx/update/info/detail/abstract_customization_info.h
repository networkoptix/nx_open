#pragma once

namespace nx {
namespace update {
namespace info {
namespace detail {

class AbstractCustomizationInfo
{
public:
    virtual ~AbstractCustomizationInfo() {}
    virtual QString name() = 0;
    virtual QList<QString> versions() = 0;
};

} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
