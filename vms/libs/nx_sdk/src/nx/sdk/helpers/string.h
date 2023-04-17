// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/i_string.h>

namespace nx::sdk {

class String: public RefCountable<IString>
{
public:
    String() = default;
    String(std::string s);

    /** @param s If null, empty string is assumed. */
    String(const char* s);

    virtual const char* str() const override;

    void setString(std::string s);

    /** @param s If null, empty string is assumed. */
    void setString(const char* s);

    int size() const;

    bool empty() const;

private:
    std::string m_string;
};

} // namespace nx::sdk
