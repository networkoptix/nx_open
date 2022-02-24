// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <utility>

namespace nx::network::aio::detail {

class AbstractDataWrapper
{
public:
    virtual ~AbstractDataWrapper() = default;
};

template<typename Data>
class DataWrapper:
    public AbstractDataWrapper
{
public:
    DataWrapper(Data data):
        m_data(std::move(data))
    {
    }

    Data take()
    {
        return std::move(m_data);
    }

private:
    Data m_data;
};

} // namespace nx::network::aio::detail
