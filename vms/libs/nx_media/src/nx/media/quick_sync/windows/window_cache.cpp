#ifdef _WIN32

#include "window_cache.h"
#include <nx/utils/log/log.h>

namespace {

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        // Allow the window to be larger than the screen
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO *minmax = (MINMAXINFO *) lParam;
            minmax->ptMaxSize.x = 8192;
            minmax->ptMaxSize.y = 8192;
            minmax->ptMaxTrackSize.x = 8192;
            minmax->ptMaxTrackSize.y = 8192;
            return 0;
        }
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc (hWnd, message, wParam, lParam);
}

class RegisterWindowClass
{
public:
    RegisterWindowClass()
    {
        WNDCLASSEX wc;
        ZeroMemory(&wc, sizeof(WNDCLASSEX));

        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = (HINSTANCE)GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = L"WindowClass";

        RegisterClassEx(&wc);
    }
    ~RegisterWindowClass()
    {
        UnregisterClass(L"WindowClass", (HINSTANCE)GetModuleHandle(NULL));
    }
};

HWND createDxWindow(int width, int height)
{
    static RegisterWindowClass windowClass;

    HWND hWnd = CreateWindowEx(NULL, L"WindowClass", L"Shared Resource - DX",
        WS_OVERLAPPEDWINDOW, 0, 0, width, height,
        NULL, NULL, (HINSTANCE)GetModuleHandle(NULL), NULL);

    if (IsWindow(hWnd))
    {
        RECT rc = { 0, 0, width, height};

        DWORD dwStyle = GetWindowLongPtr(hWnd, GWL_STYLE);
        DWORD dwExStyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
        AdjustWindowRectEx(&rc, dwStyle, FALSE, dwExStyle);
        SetWindowPos(hWnd, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER | SWP_NOMOVE);
    }
    return hWnd;
}

} // namespace

namespace nx::media::quick_sync::windows {

constexpr int kWindowCacheSize = 4;

WindowCache::~WindowCache()
{
    for (auto& window: m_windows)
        DestroyWindow(window.handle);
}

HWND WindowCache::getWindow(int width, int height)
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    for (auto& window: m_windows)
    {
        if (!window.locked && window.width == width && window.height == height)
        {
            window.locked = true;
            return window.handle;
        }
    }

    auto iter = m_windows.begin();
    while (iter != m_windows.end() && m_windows.size() >= kWindowCacheSize)
    {
        if (!iter->locked)
            m_windows.erase(iter++);
        else
            ++iter;
    }

    Window window;
    window.width = width;
    window.height = height;
    window.locked = true;
    window.handle = createDxWindow(width, height);
    m_windows.push_back(window);
    return window.handle;
}

void WindowCache::releaseWindow(HWND handle)
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    for (auto& window: m_windows)
    {
        if (window.handle == handle)
        {
            if (!window.locked)
            {
                NX_ERROR(this, "Window in invalid state: %1, resolution: %2x%3",
                    window.handle, window.width, window.height);
            }

            window.locked = false;
            return;
        }
    }
    NX_ERROR(this, "Window handle not found: %1", handle);
}

} // namespace nx::media::quick_sync::windows

#endif // _WIN32
