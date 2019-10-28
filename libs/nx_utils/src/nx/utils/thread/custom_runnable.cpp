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

