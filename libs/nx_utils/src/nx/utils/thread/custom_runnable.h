// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QRunnable>

namespace nx::utils::thread {

class NX_UTILS_API CustomRunnable: public QRunnable
{
public:
    using Callback = std::function<void ()>;

    CustomRunnable(Callback&& callback);

    virtual void run() override;

private:
    Callback m_callback;
};

} // namespace nx::utils::thread
