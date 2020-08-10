#pragma once

#ifdef _WIN32

#include <list>
#include <mutex>

#include <windows.h>

namespace nx::media::quick_sync::windows {

class WindowCache
{
public:
    ~WindowCache();
    HWND getWindow(int width, int height);
    void releaseWindow(HWND window);

private:
    struct Window {
        int width = 0;
        int height = 0;
        bool locked = false;
        HWND handle;
    };

private:
    std::list<Window> m_windows;
    std::mutex m_mutex;
};

} // namespace nx::media::quick_sync::windows

#endif // _WIN32
