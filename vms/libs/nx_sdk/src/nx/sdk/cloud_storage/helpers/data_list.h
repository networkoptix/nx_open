// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

namespace nx::sdk::cloud_storage {

template<typename Data>
class DataList
{
protected:
    DataList(std::vector<Data> data):
        m_data(std::move(data)),
        m_it(m_data.cbegin())
    {
    }

    void goToBeginning() const
    {
        m_it = m_data.cbegin();
    }

    void next() const
    {
        if (m_it != m_data.cend())
            ++m_it;
    }

    bool atEnd() const
    {
        return m_it == m_data.cend();
    }

    const Data* get() const
    {
        if (atEnd())
            return nullptr;

        return &(*m_it);
    }

private:
    std::vector<Data> m_data;
    mutable typename std::vector<Data>::const_iterator m_it;
};

} // namespace nx::sdk::cloud_storage
