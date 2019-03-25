#pragma once

#include <nx/sdk/i_list.h>

#include <nx/sdk/helpers/list.h>

namespace nx {
namespace sdk {

template<typename IItem>
class List: public RefCountable<IList<IItem>>
{
public:
    virtual int count() const override
    {
        return (int) m_items.size();
    }

    virtual IItem* at(int index) const override
    {
        if (!NX_KIT_ASSERT(index >= 0 && index < (int) m_items.size()))
            return nullptr;

        if (!NX_KIT_ASSERT(m_items[index]))
            return nullptr;

        m_items[index]->addRef();
        return m_items[index].get();
    }

    void addItem(IItem* item)
    {
        if (!NX_KIT_ASSERT(item))
            return;

        item->addRef();
        m_items.push_back(nx::sdk::toPtr(item));
    }

    void clear()
    {
        m_items.clear();
    }

private:
    std::vector<nx::sdk::Ptr<IItem>> m_items;
};

} // namespace sdk
} // namespace nx
