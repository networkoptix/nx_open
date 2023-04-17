// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/i_string_map.h>

namespace nx::sdk {

// TODO: Do something with O(N^2) complexity of lookup by index.
class StringMap: public RefCountable<IStringMap>
{
public:
    using Map = std::map<std::string, std::string>;

    StringMap() = default;

    StringMap(Map map): m_map(std::move(map)) {}

    void setItem(const std::string& key, const std::string& value);

    void clear();

    virtual int count() const override;

    /** @return Pointer that is valid only until this object is changed or deleted. */
    virtual const char* key(int i) const override;

    /** @return Pointer that is valid only until this object is changed or deleted. */
    virtual const char* value(int i) const override;

    virtual const char* value(const char* key) const override;

private:
    Map m_map;
};

} // namespace nx::sdk
