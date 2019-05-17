#pragma once

#include <vector>
#include <string>

#include <nx/sdk/i_string_list.h>
#include <nx/sdk/helpers/ref_countable.h>

namespace nx {
namespace sdk {

class StringList: public nx::sdk::RefCountable<IStringList>
{
public:
    virtual int count() const override;

    /** @return Null if index is incorrect. */
    virtual const char* at(int index) const override;

    void addString(std::string str);
    void clear();

private:
    std::vector<std::string> m_strings;
};

} // namespace sdk
} // namespace nx
