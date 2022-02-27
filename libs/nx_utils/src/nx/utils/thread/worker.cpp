// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "worker.h"

namespace nx::utils {

Worker::Worker(std::optional<size_t> maxTaskCount)
{
    m_impl.reset(new detail::Impl(maxTaskCount));
}

Worker::~Worker()
{
    m_impl->stop();
}

void Worker::post(Task task)
{
    m_impl->post(std::move(task));
}

size_t Worker::size() const
{
    return m_impl->size();
}

} // namespace nx::utils
