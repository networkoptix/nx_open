#include "system_non_client_area_remover_win.h"
#include "gdi_win.h"

#include <Windowsx.h>

#include <array>

#include <QtCore/QAbstractNativeEventFilter>
#include <QtGui/QGuiApplication>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtGui/private/qhighdpiscaling_p.h>
#include <QtWidgets/QWidget>

#include <ui/widgets/common/emulated_frame_widget.h>

#include <nx/vms/client/desktop/common/widgets/emulated_non_client_area.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

namespace {

LRESULT windowFrameSectionToHitTestResult(Qt::WindowFrameSection section)
{
    switch (section)
    {
        case Qt::NoSection:
            return HTCLIENT;

        case Qt::LeftSection:
            return HTLEFT;

        case Qt::TopLeftSection:
            return HTTOPLEFT;

        case Qt::TopSection:
            return HTTOP;

        case Qt::TopRightSection:
            return HTTOPRIGHT;

        case Qt::RightSection:
            return HTRIGHT;

        case Qt::BottomRightSection:
            return HTBOTTOMRIGHT;

        case Qt::BottomSection:
            return HTBOTTOM;

        case Qt::BottomLeftSection:
            return HTBOTTOMLEFT;

        case Qt::TitleBarArea:
            return HTCAPTION;

        default:
            NX_ASSERT(false);
            return HTNOWHERE;
    }
}

} // namespace

class SystemNonClientAreaRemover::Private:
    public QObject,
    public QAbstractNativeEventFilter
{
public:
    virtual bool nativeEventFilter(
        const QByteArray& /*eventType*/, void* message, long* result) override
    {
        const auto msg = static_cast<MSG*>(message);
        const auto widget = idToWidget.value(WId(msg->hwnd));

        if (!widget || widget->isFullScreen() || !widget->isVisible())
            return false;

        if (!NX_ASSERT(widget->windowHandle()))
            return false;

        switch (msg->message)
        {
            case WM_SHOWWINDOW:
            {
                // Update frame geometry (required only on the first show).
                SetWindowPos(msg->hwnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED
                    | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREPOSITION);

                return false;
            }

            // TODO: #vkutin Implement WM_GETMINMAXINFO.
            // Windows are still maximized as if they've got a system frame.

            case WM_NCCALCSIZE:
            {
                // Remove non-client area completely.
                *result = WVR_REDRAW;
                return true;
            }

            case WM_NCHITTEST:
            {
                // If EmulatedNonClientArea instance is found, delegate hit testing to it,
                // otherwise only check if the point is inside or outside the window.

                RECT rect;
                GetWindowRect(msg->hwnd, &rect);

                const QPoint point = QHighDpi::fromNativePixels(
                    QPoint(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)),
                    widget->windowHandle());

                const QRect geometry = QHighDpi::fromNativePixels(
                    QRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top),
                    widget->windowHandle());

                if (!geometry.contains(point))
                {
                    *result = HTNOWHERE;
                    return true;
                }

                const auto emulatedFrame =
                    widget->property(EmulatedNonClientArea::kParentPropertyName.data())
                        .value<EmulatedNonClientArea*>();

                if (emulatedFrame)
                {
                    *result = windowFrameSectionToHitTestResult(
                        emulatedFrame->hitTest(widget->windowHandle()->mapFromGlobal(point)));
                }
                else
                {
                    *result = HTCLIENT;
                }

                return true;
            }

            case WM_NCACTIVATE:
            {
                // Tell system to process activation as usual;
                // do not call DefWindowProc to avoid any painting.
                *result = TRUE;
                return true;
            }

            case WM_NCPAINT:
            {
                // Fill non-client area with QPalette::Window palette color.

                gdi::WindowDC context(msg->hwnd);
                gdi::SolidBrush brush(widget->palette().color(QPalette::Window));

                RECT rect;
                GetWindowRect(msg->hwnd, &rect);

                if (GetObjectType(HGDIOBJ(msg->wParam)) == OBJ_REGION)
                {
                    OffsetRgn(HRGN(msg->wParam), -rect.left, -rect.top);
                    SelectClipRgn(context, HRGN(msg->wParam));
                }

                OffsetRect(&rect, -rect.left, -rect.top);
                FillRect(context, &rect, brush);

                *result = 0;
                return true;
            }

            default:
                return false;
        }
    }

    virtual bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::WinIdChange)
        {
            removeFromLookups(watched);
            addToLookups(qobject_cast<QWidget*>(watched));
        }

        return QObject::eventFilter(watched, event);
    }

    void add(QWidget* widget)
    {
        if (!NX_ASSERT(widget) || widgetToId.contains(widget))
            return;

        connect(widget, &QObject::destroyed, this, &Private::removeFromLookups);
        widget->installEventFilter(this);
        addToLookups(widget);
    }

    bool remove(QWidget* widget)
    {
        if (!NX_ASSERT(widget) || !widgetToId.contains(widget))
            return false;

        widget->removeEventFilter(this);
        removeFromLookups(widget);
        return true;
    }

private:
    void addToLookups(QWidget* widget)
    {
        if (!NX_ASSERT(widget))
            return;

        const WId id = widget->internalWinId();
        widgetToId[widget] = id;
        if (id)
            idToWidget[id] = widget;
    }

    void removeFromLookups(QObject* widget)
    {
        idToWidget.remove(widgetToId.take(static_cast<QWidget*>(widget)));
    }

private:
    QHash<WId, QWidget*> idToWidget;
    QHash<QWidget*, WId> widgetToId;
};

SystemNonClientAreaRemover::SystemNonClientAreaRemover():
    d(new Private())
{
    NX_CRITICAL(qApp, "SystemNonClientAreaRemover requires QCoreApplication instance to exist");
    qApp->installNativeEventFilter(d.get());
}

SystemNonClientAreaRemover::~SystemNonClientAreaRemover()
{
    // Required here for forward-declared scoped pointer destruction.
}

SystemNonClientAreaRemover& SystemNonClientAreaRemover::instance()
{
    static SystemNonClientAreaRemover instance;
    return instance;
}

void SystemNonClientAreaRemover::apply(QWidget* widget)
{
    d->add(widget);
}

bool SystemNonClientAreaRemover::restore(QWidget* widget)
{
    return d->remove(widget);
}

} // namespace nx::vms::client::desktop
