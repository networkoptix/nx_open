#include "screen_saver_disabler_win.h"

ScreenSaverDisabler::ScreenSaverDisabler()
    : m_powerOffTimeout(0),
      m_powerOffTimeout(0),
      m_screenSaveTimeout(0)
{
}

ScreenSaverDisabler::~ScreenSaverDisabler()
{
    restore();
}

void ScreenSaverDisabler::disable()
{
    SystemParametersInfo(SPI_GETLOWPOWERTIMEOUT, 0,
                         &m_powerOffTimeout, 0);
    if (0 != m_powerOffTimeout) {
        SystemParametersInfo(SPI_SETLOWPOWERTIMEOUT, 0, NULL, 0);
    }
    SystemParametersInfo(SPI_GETPOWEROFFTIMEOUT, 0,
                         &m_powerOffTimeout, 0);
    if (0 != m_powerOffTimeout) {
        SystemParametersInfo(SPI_SETPOWEROFFTIMEOUT, 0, NULL, 0);
    }
    SystemParametersInfo(SPI_GETSCREENSAVETIMEOUT, 0,
                         &m_screenSaveTimeout, 0);
    if (0 != m_screenSaveTimeout) {
        SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, 0, NULL, 0);
    }
}

void ScreenSaverDisabler::restore()
{
    if (0 != m_powerOffTimeout) {
        SystemParametersInfo(SPI_SETLOWPOWERTIMEOUT,
                             m_powerOffTimeout, NULL, 0);
    }
    if (0 != m_powerOffTimeout) {
        SystemParametersInfo(SPI_SETPOWEROFFTIMEOUT,
                             m_powerOffTimeout, NULL, 0);
    }
    if (0 != m_screenSaveTimeout) {
        SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT,
                             m_screenSaveTimeout, NULL, 0);
    }
}
