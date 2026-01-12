// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_backend.h"

namespace nx::vms::client::core {
namespace timeline {

template<typename DataList>
class AbstractProxyBackend: public AbstractBackend<DataList>
{
public:
    explicit AbstractProxyBackend(const BackendPtr<DataList>& source):
        m_source(source)
    {
        NX_ASSERT(source);
    }

    BackendPtr<DataList> source() const
    {
        return m_source;
    }

    virtual QnResourcePtr resource() const override
    {
        return m_source ? m_source->resource() : QnResourcePtr{};
    }

private:
    const BackendPtr<DataList> m_source;
};

} // namespace timeline
} // namespace nx::vms::client::core
