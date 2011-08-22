#ifndef SCREEN_SAVER_DISABLER_H
#define SCREEN_SAVER_DISABLER_H

class ScreenSaverDisabler
{
public:
    ScreenSaverDisabler();
    ~ScreenSaverDisabler();

    void disable();
    void restore();

private:
    UINT m_lowPowerTimeout;
    UINT m_powerOffTimeout;
    UINT m_screenSaveTimeout;
};

#endif // SCREEN_SAVER_DISABLER_H
