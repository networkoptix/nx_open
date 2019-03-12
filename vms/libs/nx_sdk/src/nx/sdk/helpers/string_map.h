#pragma once

#include <map>

#include <nx/sdk/helpers/ref_countable.h>

#include <nx/sdk/i_string_map.h>

namespace nx {
namespace sdk {

// TODO: Do something with O(N^2) complexity of lookup by index.
class StringMap: public RefCountable<IStringMap>
{
    using Map = std::map<std::string, std::string>;

public:
    void addItem(const std::string& key, const std::string& value);

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

} // namespace sdk
} // namespace nx
