#pragma once

#if !defined(Q_OS_WIN)
#error This is Windows-only header.
#endif

namespace nx::vms::client::desktop {
namespace gdi {

class WindowDC final
{
    const HDC m_handle;

public:
    explicit WindowDC(HWND window): m_handle(GetWindowDC(window)) {}
    ~WindowDC() { ReleaseDC(WindowFromDC(m_handle), m_handle); }

    operator HDC() const { return m_handle; }
};

class ClientDC final
{
    const HDC m_handle;

public:
    explicit ClientDC(HWND window): m_handle(GetDC(window)) {}
    ~ClientDC() { ReleaseDC(WindowFromDC(m_handle), m_handle); }

    operator HDC() const { return m_handle; }
};

class SolidBrush final
{
    const HBRUSH m_handle;

public:
    explicit SolidBrush(const QColor& color):
        m_handle(CreateSolidBrush(RGB(color.red(), color.green(), color.blue()))) {}

    ~SolidBrush() { DeleteObject(m_handle); }

    operator HBRUSH() const { return m_handle; }
};

} // namespace gdi
} // namespace nx::vms::client::desktop
