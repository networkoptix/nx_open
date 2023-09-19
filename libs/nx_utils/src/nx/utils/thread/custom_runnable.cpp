// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "custom_runnable.h"

namespace nx::utils::thread {

CustomRunnable::CustomRunnable(Callback&& callback):
    m_callback(callback)
{
}

void CustomRunnable::run()
{
    if (m_callback)
        m_callback();
}

} // namespace nx::utils::thread
