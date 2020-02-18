#pragma once

#include <va/va.h>

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
