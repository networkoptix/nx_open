// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/i_string_list.h>

namespace nx::sdk {

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

} // namespace nx::sdk
