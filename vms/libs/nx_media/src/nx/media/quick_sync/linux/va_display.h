#pragma once

#ifdef __linux__

#include <va/va.h>

namespace nx::media::quick_sync::linux {

class VaDisplay
{
public:
    static VADisplay getDisplay()
    {
        static VaDisplay vaDisplay;
        return vaDisplay.m_display;
    }

private:
    VaDisplay();
    ~VaDisplay();

private:
    VADisplay m_display;
};

} // namespace nx::media::quick_sync::linux

#endif // __linux__
