#include "dwm.h"

#ifndef Q_MOC_RUN
#include <boost/type_traits/remove_pointer.hpp>
#endif

#include <QtCore/QLibrary>
#include <QtWidgets/QWidget>

#include <utils/common/warnings.h>

#include <utils/common/invocation_event.h>

#if defined(QN_HAS_DWM)
#include <QtWidgets/private/qwidget_p.h>

#include <qt_windows.h>
#include <utils/qt5port_win.h>

#define NOMINMAX
#include <Windows.h>
#include <WindowsX.h>

/* The following definitions are from <dwmapi.h>.
 * They are here so that everything would compile even if we don't have that include file. */

#define WM_DWMCOMPOSITIONCHANGED        0x031E      /**< Composition changed window message. */

struct _DWM_BLURBEHIND {
    DWORD dwFlags;
    BOOL fEnable;
    HRGN hRgnBlur;
    BOOL fTransitionOnMaximized;
};

#define DWM_BB_ENABLE                   0x00000001  /**< fEnable has been specified. */
#define DWM_BB_BLURREGION               0x00000002  /**< hRgnBlur has been specified. */
#define DWM_BB_TRANSITIONONMAXIMIZED    0x00000004  /**< fTransitionOnMaximized has been specified. */

struct _MARGINS {
    int cxLeftWidth;
    int cxRightWidth;
    int cyTopHeight;
    int cyBottomHeight;
};

typedef enum _DWMWINDOWATTRIBUTE {
    DWMWA_NCRENDERING_ENABLED = 1,
    DWMWA_NCRENDERING_POLICY,
    DWMWA_TRANSITIONS_FORCEDISABLED,
    DWMWA_ALLOW_NCPAINT,
    DWMWA_CAPTION_BUTTON_BOUNDS,
    DWMWA_NONCLIENT_RTL_LAYOUT,
    DWMWA_FORCE_ICONIC_REPRESENTATION,
    DWMWA_FLIP3D_POLICY,
    DWMWA_EXTENDED_FRAME_BOUNDS,
    DWMWA_HAS_ICONIC_BITMAP,
    DWMWA_DISALLOW_PEEK,
    DWMWA_EXCLUDED_FROM_PEEK,
    DWMWA_LAST
} DWMWINDOWATTRIBUTE;

typedef enum _DWMNCRENDERINGPOLICY {
    DWMNCRP_USEWINDOWSTYLE,
    DWMNCRP_DISABLED,
    DWMNCRP_ENABLED,
    DWMNCRP_LAST
} DWMNCRENDERINGPOLICY;

typedef boost::remove_pointer<LRESULT>::type RESULT;

typedef HRESULT (WINAPI *PtrDwmIsCompositionEnabled)(BOOL* pfEnabled);
typedef HRESULT (WINAPI *PtrDwmExtendFrameIntoClientArea)(HWND hWnd, const _MARGINS *pMarInset);
typedef HRESULT (WINAPI *PtrDwmEnableBlurBehindWindow)(HWND hWnd, const _DWM_BLURBEHIND *pBlurBehind);
typedef HRESULT (WINAPI *PtrDwmGetColorizationColor)(DWORD *pcrColorization, BOOL *pfOpaqueBlend);
typedef BOOL (WINAPI *PtrDwmDefWindowProc)(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *plResult);
typedef HRESULT (WINAPI *PtrDwmSetWindowAttribute)(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);
#endif // QN_HAS_DWM

enum QnDwmInvocation {
    AdjustPositionInvocation = 0x7591
};

class QnDwmPrivate {
public:
    QnDwmPrivate(QnDwm *qq): q(qq) {}

    virtual ~QnDwmPrivate() {}

    void init(QWidget *widget);

#ifdef QN_HAS_DWM
    void updateFrameStrut();
    bool winEvent(MSG *message, long *result);
    bool calcSizeEvent(MSG *message, long *result);
    bool compositionChangedEvent(MSG *message, long *result);
    bool activateEvent(MSG *message, long *result);
    bool ncPaintEvent(MSG *message, long *result);
    bool getMinMaxInfoEvent(MSG *message, long *result);
#endif // QN_HAS_DWM

    static bool isSupported();

public:
    /** Public counterpart. */
    QnDwm *q;

    /** Widget that we're working on. */
    QWidget *widget;

    /** Whether this api instance is functional. */
    bool hasApi;

#ifdef QN_HAS_DWM
    /** Whether DWM is available. */
    bool hasDwm;

    /* Function pointers for functions from dwmapi.dll. */
    PtrDwmIsCompositionEnabled dwmIsCompositionEnabled;
    PtrDwmEnableBlurBehindWindow dwmEnableBlurBehindWindow;
    PtrDwmExtendFrameIntoClientArea dwmExtendFrameIntoClientArea;
    PtrDwmGetColorizationColor dwmGetColorizationColor;
    PtrDwmDefWindowProc dwmDefWindowProc;
    PtrDwmSetWindowAttribute dwmSetWindowAttribute;

    /** Widget frame margins specified by the user. */
    QMargins userFrameMargins;

    bool overrideFrameMargins;
#endif // QN_HAS_DWM
};

bool QnDwmPrivate::isSupported() {
    bool result;

#ifdef QN_HAS_DWM
    /* We're using private Qt functions, so we have to check that the right runtime is used. */
    result = !qstrcmp(qVersion(), QT_VERSION_STR);
    if(!result)
        qnWarning("Compile-time and run-time Qt versions differ (%1 != %2), DWM will be disabled.", qVersion(), QT_VERSION_STR);
#else
    result = false;
#endif // QN_HAS_DWM

    return result;
}

Q_GLOBAL_STATIC_WITH_ARGS(bool, qn_dwm_isSupported, (QnDwmPrivate::isSupported()));

void QnDwmPrivate::init(QWidget *widget) {
    this->widget = widget;
    hasApi = widget != NULL && qn_dwm_isSupported();

#ifdef QN_HAS_DWM
    QLibrary dwmLib(lit("dwmapi"));
    dwmIsCompositionEnabled         = (PtrDwmIsCompositionEnabled)      dwmLib.resolve("DwmIsCompositionEnabled");
    dwmExtendFrameIntoClientArea    = (PtrDwmExtendFrameIntoClientArea) dwmLib.resolve("DwmExtendFrameIntoClientArea");
    dwmEnableBlurBehindWindow       = (PtrDwmEnableBlurBehindWindow)    dwmLib.resolve("DwmEnableBlurBehindWindow");
    dwmGetColorizationColor         = (PtrDwmGetColorizationColor)      dwmLib.resolve("DwmGetColorizationColor");
    dwmDefWindowProc                = (PtrDwmDefWindowProc)             dwmLib.resolve("DwmDefWindowProc");
    dwmSetWindowAttribute           = (PtrDwmSetWindowAttribute)        dwmLib.resolve("DwmSetWindowAttribute");

    hasDwm =
        hasApi &&
        dwmIsCompositionEnabled != NULL &&
        dwmExtendFrameIntoClientArea != NULL &&
        dwmEnableBlurBehindWindow != NULL &&
        dwmGetColorizationColor != NULL &&
        dwmDefWindowProc != NULL &&
        dwmSetWindowAttribute != NULL;

    overrideFrameMargins = false;
#endif // QN_HAS_DWM
}

#ifdef QN_HAS_DWM
void QnDwmPrivate::updateFrameStrut() {
    QWidgetPrivate *wd = qt_widget_private(widget);
    QTLWExtra *tlwExtra = wd->maybeTopData();
    if(tlwExtra != NULL) {
        QMargins margins = q->currentFrameMargins();
        tlwExtra->frameStrut = QRect(
            QPoint(
                margins.left(),
                margins.top()
            ),
            QPoint(
                margins.right(),
                margins.bottom()
            )
        );

        qt_qwidget_data(widget)->fstrut_dirty = false;
    }
}
#endif // QN_HAS_DWM

QnDwm::QnDwm(QWidget *widget):
    QObject(widget),
    d(new QnDwmPrivate(this))
{
    if(widget == NULL)
        qnNullWarning(widget);

    d->init(widget);
}

QnDwm::~QnDwm() {
    delete d;
    d = NULL;
}

bool QnDwm::isSupported() {
    return *qn_dwm_isSupported();
}

bool QnDwm::enableBlurBehindWindow(bool enable) {
    if(!d->hasApi)
        return false;

#ifdef QN_HAS_DWM
    if(!d->hasDwm)
        return false;

    _DWM_BLURBEHIND blurBehind = {0};
    HRESULT status = S_OK;
    blurBehind.fEnable = enable;
    blurBehind.dwFlags = DWM_BB_ENABLE;
    blurBehind.hRgnBlur = NULL;

    status = d->dwmEnableBlurBehindWindow(widToHwnd(d->widget->winId()), &blurBehind);

    return SUCCEEDED(status);
#else
    Q_UNUSED(enable)
    return false;
#endif // QN_HAS_DWM
}

bool QnDwm::isCompositionEnabled() const {
    if(!d->hasApi)
        return false;

#ifdef QN_HAS_DWM
    if (!d->hasDwm)
        return false;

    HRESULT status = S_OK;
    BOOL result = false;

    status = d->dwmIsCompositionEnabled(&result);

    if (SUCCEEDED(status)) {
        return result;
    } else {
        return false;
    }
#else
    return false;
#endif // QN_HAS_DWM
}

bool QnDwm::extendFrameIntoClientArea(const QMargins &margins) {
    if(!d->hasApi)
        return false;

#ifdef QN_HAS_DWM
    if(!d->hasDwm)
        return false;

    HRESULT status = S_OK;
    _MARGINS winMargins;
    winMargins.cxLeftWidth      = margins.left();
    winMargins.cxRightWidth     = margins.right();
    winMargins.cyBottomHeight   = margins.bottom();
    winMargins.cyTopHeight      = margins.top();

    status = d->dwmExtendFrameIntoClientArea(widToHwnd(d->widget->winId()), &winMargins);

    if(SUCCEEDED(status)) {
        /* Make sure that the extended frame is visible by setting the WNDCLASS's
         * background brush to transparent black.
         * This also eliminates artifacts with white (default background color)
         * fields appearing in client area when the window is resized. */
        HGDIOBJ blackBrush = GetStockObject(BLACK_BRUSH);
        DWORD result = SetClassLongPtr(widToHwnd(d->widget->winId()), GCLP_HBRBACKGROUND, (LONG_PTR) blackBrush);
        if(result == 0) {
            DWORD error = GetLastError();
            if(error != 0)
                qnWarning("Failed to set window background brush to transparent, error code '%1'.", error);
        }
    }

    return SUCCEEDED(status);
#else
    Q_UNUSED(margins)
    return false;
#endif // QN_HAS_DWM
}

QMargins QnDwm::themeFrameMargins() const {
    if(!d->hasApi)
        return QMargins(-1, -1, -1, -1);

#ifdef QN_HAS_DWM
    int frameX, frameY;
    if(d->widget->windowFlags() & Qt::FramelessWindowHint) {
        frameX = 0;
        frameY = 0;
    } else if(d->widget->windowFlags() & Qt::MSWindowsFixedSizeDialogHint) {
        frameX = GetSystemMetrics(SM_CXFIXEDFRAME);
        frameY = GetSystemMetrics(SM_CYFIXEDFRAME);
    } else {
        frameX = GetSystemMetrics(SM_CXSIZEFRAME);
        frameY = GetSystemMetrics(SM_CYSIZEFRAME);
    }

    return QMargins(frameX, frameY, frameX, frameY);
#else
    return QMargins(-1, -1, -1, -1);
#endif // QN_HAS_DWM
}

int QnDwm::themeTitleBarHeight() const {
    if(!d->hasApi)
        return -1;

#ifdef QN_HAS_DWM
    if((d->widget->windowFlags() & Qt::FramelessWindowHint) || (d->widget->windowFlags() & Qt::X11BypassWindowManagerHint)) {
        return 0;
    } else if(d->widget->windowFlags() & Qt::MSWindowsFixedSizeDialogHint) {
        return GetSystemMetrics(SM_CYSMCAPTION);
    } else {
        return GetSystemMetrics(SM_CYCAPTION);
    }
#else
    return -1;
#endif // QN_HAS_DWM
}

QMargins QnDwm::currentFrameMargins() const {
    QMargins errorValue(-1, -1, -1, -1);

    if(!d->hasApi)
        return errorValue;

#ifdef QN_HAS_DWM
    HWND hwnd = widToHwnd(d->widget->winId());
    BOOL status = S_OK;

    RECT clientRect;
    status = GetClientRect(widToHwnd(d->widget->winId()), &clientRect);
    if(!SUCCEEDED(status))
        return errorValue;

    RECT windowRect;
    status = GetWindowRect(hwnd, &windowRect);
    if(!SUCCEEDED(status))
        return errorValue;

    POINT clientTopLeft = {0, 0};
    status = ClientToScreen(hwnd, &clientTopLeft);
    if(!SUCCEEDED(status))
        return errorValue;

    return QMargins(
        (clientTopLeft.x + clientRect.left) - windowRect.left,
        (clientTopLeft.y + clientRect.top) - windowRect.top,
        windowRect.right - (clientTopLeft.x + clientRect.right),
        windowRect.bottom - (clientTopLeft.y + clientRect.bottom)
    );
#else
    return QMargins(-1, -1, -1, -1);
#endif // QN_HAS_DWM
}

bool QnDwm::setCurrentFrameMargins(const QMargins &margins) {
    if(!d->hasApi)
        return false;

#ifdef QN_HAS_DWM
    HWND hwnd = widToHwnd(d->widget->winId());
    BOOL status = S_OK;

    /* Store supplied frame margins. They will be used in WM_NCCALCSIZE handler. */
    d->userFrameMargins = margins;
    d->overrideFrameMargins = true;

    status = SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    if(!SUCCEEDED(status))
        return false;

    d->updateFrameStrut();

    return true;
#else
    Q_UNUSED(margins)
    return false;
#endif // QN_HAS_DWM
}

bool QnDwm::widgetEvent(QEvent *event) {
    Q_UNUSED(event);

    if(!d->hasApi)
        return false;

#ifdef QN_HAS_DWM
    if(d->overrideFrameMargins) {
        /* Qt calculates frame margins based on window's style,
         * not on actual margins specified by WM_NCCALCSIZE. We fix that. */
        QWidgetData *data = qt_qwidget_data(d->widget);
        if(data->fstrut_dirty)
            d->updateFrameStrut();

        /* This is needed because when going out of full screen, position calculation
         * breaks despite the fact that frame struts are set. Moving the widget fixes that.
         * However, we cannot move it right away, because if it was restored from minimized state,
         * then its position is not yet initialized. */
        if(event->type() == QEvent::WindowStateChange) {
            QWindowStateChangeEvent *e = static_cast<QWindowStateChangeEvent *>(event);
            if((e->oldState() & Qt::WindowFullScreen) && !(d->widget->windowState() & Qt::WindowFullScreen))
                QApplication::postEvent(d->widget, new QnInvocationEvent(AdjustPositionInvocation), Qt::LowEventPriority);
        }

        if(event->type() == QnEvent::Invocation && static_cast<QnInvocationEvent *>(event)->id() == AdjustPositionInvocation)
            d->widget->move(d->widget->pos());
    }
#endif // QN_HAS_DWM

    return false;
}


bool QnDwm::widgetNativeEvent(const QByteArray &eventType, void *message, long *result) {
    Q_UNUSED(eventType)
#ifdef QN_HAS_DWM
    if(d->widget == NULL)
        return false;

    return d->winEvent(static_cast<MSG *>(message), result);
#else
    Q_UNUSED(message)
    Q_UNUSED(result)
    return false;
#endif // QN_HAS_DWM
}

#ifdef QN_HAS_DWM
bool QnDwmPrivate::winEvent(MSG *message, long *result) {
    if(hasDwm) {
        RESULT localResult;
        BOOL handled = dwmDefWindowProc(message->hwnd, message->message, message->wParam, message->lParam, &localResult);
        if (handled) {
            *result = localResult;
            return true;
        }
    }

    switch(message->message) {
    case WM_NCCALCSIZE:             return calcSizeEvent(message, result);
    case WM_DWMCOMPOSITIONCHANGED:  return compositionChangedEvent(message, result);
    case WM_NCACTIVATE:             return activateEvent(message, result);
    case WM_NCPAINT:                return ncPaintEvent(message, result);
    case WM_GETMINMAXINFO:          return getMinMaxInfoEvent(message, result);
    default:                        return false;
    }
}

bool QnDwmPrivate::calcSizeEvent(MSG *message, long *result) {
    if(!overrideFrameMargins)
        return false;

    /* From WM_NCCALCSIZE docs:
     *
     * If wParam is TRUE, it specifies that the application should indicate which
     * part of the client area contains valid information.
     * The system copies the valid information to the specified area within the new client area.
     *
     * If wParam is FALSE, the application does not need to indicate the valid part of the client area. */
    if(!message->wParam)
        return false;

    NCCALCSIZE_PARAMS *params = (NCCALCSIZE_PARAMS *) message->lParam;

    /* Store input. */
    RECT targetGeometry = params->rgrc[0];
    RECT sourceGeometry = params->rgrc[1];

    /* Prepare output.
     *
     * 0 - the coordinates of the new client rectangle resulting from the move or resize.
     * 1 - the valid destination rectangle.
     * 2 - the valid source rectangle. */
    params->rgrc[1] = targetGeometry;
    params->rgrc[2] = sourceGeometry;

    /* New client geometry. */
    params->rgrc[0].top      += userFrameMargins.top();
    params->rgrc[0].bottom   -= userFrameMargins.bottom();
    params->rgrc[0].left     += userFrameMargins.left();
    params->rgrc[0].right    -= userFrameMargins.right();

    *result = WVR_REDRAW;
    return true;
}

bool QnDwmPrivate::compositionChangedEvent(MSG *message, long *result) {
    Q_UNUSED(message);

    emit q->compositionChanged();

    /* We also need to repaint the window. */
    widget->update();

    *result = 0;
    return false; /* It's OK to let it fall through. */
}

bool QnDwmPrivate::activateEvent(MSG *message, long *result) {
    if(overrideFrameMargins)
        message->lParam = -1; /* Don't repaint the frame in default handler. It causes frame flickering. */

    *result = DefWindowProc(message->hwnd, message->message, message->wParam, message->lParam);
    return true;
}

bool QnDwmPrivate::ncPaintEvent(MSG *message, long *result) {
    if(!overrideFrameMargins)
        return false;

    HDC hdc = GetWindowDC(message->hwnd);

    bool deleteHrgn = false;
    HRGN hrgn = 0;
    if(message->wParam == 1) {
        RECT rect;
        GetClipBox(hdc, &rect);
        hrgn = CreateRectRgnIndirect(&rect);
        deleteHrgn = true;
    } else {
        hrgn = (HRGN) message->wParam;
        deleteHrgn = false;
    }

    HRGN rectHrgn;
    RECT rect;

    /* Calculate client rect delta relative to window rect. */
    POINT clientTopLeft;
    clientTopLeft.x = 0;
    clientTopLeft.y = 0;
    ClientToScreen(message->hwnd, &clientTopLeft);

    RECT windowRect;
    GetWindowRect(message->hwnd, &windowRect);

    POINT clientDelta;
    clientDelta.x = clientTopLeft.x - windowRect.left;
    clientDelta.y = clientTopLeft.y - windowRect.top;

    /* Construct client rect in window coordinates. */
    GetClientRect(message->hwnd, &rect);
    rect.left   += clientDelta.x;
    rect.top    += clientDelta.y;
    rect.right  += clientDelta.x;
    rect.bottom += clientDelta.y;

    /* Combine with region. */
    rectHrgn = CreateRectRgnIndirect(&rect);

    HRGN resultHrgn = CreateRectRgn(0, 0, 0, 0);
    int status = CombineRgn(resultHrgn, hrgn, rectHrgn, RGN_DIFF);
    Q_UNUSED(status);

    DeleteObject(rectHrgn);
    if(deleteHrgn)
        DeleteObject(hrgn);

    hrgn = resultHrgn;
    deleteHrgn = true;

    HBRUSH hbr = (HBRUSH) GetStockObject(BLACK_BRUSH);
    SelectBrush(hdc, hbr);
    PaintRgn(hdc, hrgn);
    DeleteObject(hbr);

    if(deleteHrgn)
        DeleteObject(hrgn);

    ReleaseDC(message->hwnd, hdc);

    /* An application should return zero if it processes this message. */
    *result = 0;
    return true;
}

bool QnDwmPrivate::getMinMaxInfoEvent(MSG *message, long *result) {
    Q_UNUSED(result);
    if(!overrideFrameMargins)
        return false; /* Default codepath is OK. */

    /* Adjust the maximized size and position to fit the work area of the correct monitor.
     *
     * If we don't do this, customized window will overlap the task bar when maximized.
     * See http://blogs.msdn.com/b/llobo/archive/2006/08/01/684535.aspx. */
    HMONITOR monitor = MonitorFromWindow(message->hwnd, MONITOR_DEFAULTTONEAREST);
    if(!monitor)
        return false;

    MONITORINFO monitorInfo;
    monitorInfo.cbSize = sizeof(MONITORINFO);
    if(GetMonitorInfoW(monitor, &monitorInfo) == 0)
        return false;

    MINMAXINFO *mmi = (MINMAXINFO *) message->lParam;
    mmi->ptMaxPosition.x = (monitorInfo.rcWork.left - monitorInfo.rcMonitor.left) - userFrameMargins.left();
    mmi->ptMaxPosition.y = (monitorInfo.rcWork.top - monitorInfo.rcMonitor.top) - userFrameMargins.top();
    mmi->ptMaxSize.x = (monitorInfo.rcWork.right - monitorInfo.rcWork.left) - (userFrameMargins.left() + userFrameMargins.right());
    mmi->ptMaxSize.y = (monitorInfo.rcWork.bottom - monitorInfo.rcWork.top) - (userFrameMargins.top() + userFrameMargins.bottom());

    return false;
}

#endif // QN_HAS_DWM
