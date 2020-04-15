
#ifdef __linux__

#include "va_display.h"

#include <nx/utils/log/log.h>

#include <va/va_glx.h>

namespace nx::media::quick_sync::linux {

VaDisplay::~VaDisplay()
{
    if (m_display)
        vaTerminate(m_display);
}

VaDisplay::VaDisplay()
{
    m_display = vaGetDisplayGLX(XOpenDisplay(":0.0"));
    if (!m_display)
    {
        NX_DEBUG(this, "Cannot create VADisplay");
        return;
    }

    int major, minor;
    VAStatus status = vaInitialize(m_display, &major, &minor);
    if (status != VA_STATUS_SUCCESS)
    {
        vaTerminate(m_display);
        m_display = nullptr;
        NX_DEBUG(this, "Cannot initialize VA-API: %1", status);
        return;
    }
}

} // namespace nx::media::quick_sync::linux

#endif // __linux__
