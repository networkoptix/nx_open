#pragma once

#include <vector>
#include <string>

#include <nx/sdk/common.h>

namespace nx {
namespace sdk {

class CommonStringList: public IStringList
{
public:
    virtual int count() const override;
    virtual const char* at(int index) const override;

    void addString(std::string str);
    void clear();

private:
    std::vector<std::string> m_strings;
};

} // namespace sdk
} // namespace nx
