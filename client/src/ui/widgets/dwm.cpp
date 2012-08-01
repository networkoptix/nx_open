#include "dwm.h"
#include <QLibrary>
#include <QWidget>
#include <utils/common/warnings.h>
#include <utils/common/mpl.h>
#include <ui/events/invocation_event.h>

#ifdef Q_OS_WIN
#include <QtGui/private/qwidget_p.h>
#include <qt_windows.h>
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

typedef remove_pointer<LRESULT>::type RESULT;

typedef HRESULT (WINAPI *PtrDwmIsCompositionEnabled)(BOOL* pfEnabled);
typedef HRESULT (WINAPI *PtrDwmExtendFrameIntoClientArea)(HWND hWnd, const _MARGINS *pMarInset);
typedef HRESULT (WINAPI *PtrDwmEnableBlurBehindWindow)(HWND hWnd, const _DWM_BLURBEHIND *pBlurBehind);
typedef HRESULT (WINAPI *PtrDwmGetColorizationColor)(DWORD *pcrColorization, BOOL *pfOpaqueBlend);
typedef BOOL (WINAPI *PtrDwmDefWindowProc)(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *plResult);
typedef HRESULT (WINAPI *PtrDwmSetWindowAttribute)(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);
#endif // Q_OS_WIN

enum QnDwmInvocation {
    AdjustPositionInvocation
};

class QnDwmPrivate {
public:
    QnDwmPrivate(QnDwm *qq): q(qq) {}

    virtual ~QnDwmPrivate() {}

    void init(QWidget *widget);

#ifdef Q_OS_WIN
    void updateFrameStrut();
    bool hitTestEvent(MSG *message, long *result);
    bool calcSizeEvent(MSG *message, long *result);
    bool compositionChangedEvent(MSG *message, long *result);
    bool activateEvent(MSG *message, long *result);
    bool ncPaintEvent(MSG *message, long *result);
    bool sysCommandEvent(MSG *message, long *result);
    bool eraseBackground(MSG *message, long *result);
#endif

    static bool isSupported();

public:
    /** Public counterpart. */
    QnDwm *q;

    /** Widget that we're working on. */
    QWidget *widget;

    /** Whether this api instance is functional. */
    bool hasApi;

#ifdef Q_OS_WIN
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

    QMargins nonErasableContentMargins;

    bool overrideFrameMargins;
    bool systemPaintWindow;
    bool transparentErasing;
    bool titleBarDrag;
#endif

    bool emulateFrame;
    QMargins emulatedFrameMargins;
    int emulatedTitleBarHeight;
};

bool QnDwmPrivate::isSupported() {
    bool result;

#ifdef Q_OS_WIN
    /* We're using private Qt functions, so we have to check that the right runtime is used. */
    result = !qstrcmp(qVersion(), QT_VERSION_STR);
    if(!result)
        qnWarning("Compile-time and run-time Qt versions differ (%1 != %2), DWM will be disabled.", qVersion(), QT_VERSION_STR);
#else
    result = false;
#endif

    return result;
}

Q_GLOBAL_STATIC_WITH_INITIALIZER(bool, qn_dwm_isSupported, {
    *x = QnDwmPrivate::isSupported();
});

void QnDwmPrivate::init(QWidget *widget) {
    this->widget = widget;
    hasApi = widget != NULL && qn_dwm_isSupported();

#ifdef Q_OS_WIN
    QLibrary dwmLib(QString::fromAscii("dwmapi"));
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
    systemPaintWindow = true;
    transparentErasing = false;
    titleBarDrag = true;
#endif

    emulateFrame = false;
    emulatedTitleBarHeight = 0;
}

#ifdef Q_OS_WIN
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
#endif

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
    return qn_dwm_isSupported();
}

bool QnDwm::enableSystemWindowPainting(bool enable) {
    if(!d->hasApi)
        return false;

#ifdef Q_OS_WIN
    d->systemPaintWindow = enable;
    return true;
#else
    return false;
#endif
}

bool QnDwm::enableTransparentErasing(bool enable) {
    if(!d->hasApi)
        return false;

#ifdef Q_OS_WIN
    d->transparentErasing = enable;
    return true;
#else
    return false;
#endif
}

bool QnDwm::setNonErasableContentMargins(const QMargins &margins) {
    if(!d->hasApi)
        return false;

#ifdef Q_OS_WIN
    d->nonErasableContentMargins = margins;
    return true;
#else
    return false;
#endif
}

bool QnDwm::enableBlurBehindWindow(bool enable) {
    if(!d->hasApi)
        return false;

#ifdef Q_OS_WIN
    if(!d->hasDwm)
        return false;

    _DWM_BLURBEHIND blurBehind = {0};
    HRESULT status = S_OK;
    blurBehind.fEnable = enable;
    blurBehind.dwFlags = DWM_BB_ENABLE;
    blurBehind.hRgnBlur = NULL;
    
    status = d->dwmEnableBlurBehindWindow(d->widget->winId(), &blurBehind);
    
    return SUCCEEDED(status);
#else
    return false;
#endif
}

bool QnDwm::isCompositionEnabled() const {
    if(!d->hasApi)
        return false;

#ifdef Q_OS_WIN
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
#endif
}

bool QnDwm::extendFrameIntoClientArea(const QMargins &margins) {
    if(!d->hasApi)
        return false;

#ifdef Q_OS_WIN
    if(!d->hasDwm)
        return false;

    HRESULT status = S_OK;
    _MARGINS winMargins;
    winMargins.cxLeftWidth      = margins.left();
    winMargins.cxRightWidth     = margins.right();
    winMargins.cyBottomHeight   = margins.bottom();
    winMargins.cyTopHeight      = margins.top();
    
    status = d->dwmExtendFrameIntoClientArea(d->widget->winId(), &winMargins);
    
    if(SUCCEEDED(status)) {
        /* Make sure that the extended frame is visible by setting the WNDCLASS's
         * background brush to transparent black. 
         * This also eliminates artifacts with white (default background color) 
         * fields appearing in client area when the window is resized. */
        HGDIOBJ blackBrush = GetStockObject(BLACK_BRUSH);
        DWORD result = SetClassLongPtr(d->widget->winId(), GCLP_HBRBACKGROUND, (LONG_PTR) blackBrush);
        if(result == 0) {
            DWORD error = GetLastError();
            if(error != 0)
                qnWarning("Failed to set window background brush to transparent, error code '%1'.", error);
        }
    }

    return SUCCEEDED(status);
#else
    return false;
#endif
}

QMargins QnDwm::themeFrameMargins() const {
    if(!d->hasApi)
        return QMargins(-1, -1, -1, -1);

#ifdef Q_OS_WIN
    int frameX, frameY;
    if((d->widget->windowFlags() & Qt::FramelessWindowHint) || (d->widget->windowFlags() & Qt::X11BypassWindowManagerHint)) {
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
#endif
}

int QnDwm::themeTitleBarHeight() const {
    if(!d->hasApi)
        return -1;

#ifdef Q_OS_WIN
    if((d->widget->windowFlags() & Qt::FramelessWindowHint) || (d->widget->windowFlags() & Qt::X11BypassWindowManagerHint)) {
        return 0;
    } else if(d->widget->windowFlags() & Qt::MSWindowsFixedSizeDialogHint) {
        return GetSystemMetrics(SM_CYSMCAPTION);
    } else {
        return GetSystemMetrics(SM_CYCAPTION);
    }
#else
    return -1;
#endif
}

bool QnDwm::enableFrameEmulation(bool enable) {
    if(!d->hasApi)
        return false;

    if(d->emulateFrame == enable)
        return true;

#ifdef Q_OS_WIN
    d->emulateFrame = enable;
    return true;
#else
    return false;
#endif
}

bool QnDwm::isFrameEmulated() const {
    return d->emulateFrame;
}

void QnDwm::setEmulatedFrameMargins(const QMargins &margins) {
    d->emulatedFrameMargins = margins;
}

QMargins QnDwm::emulatedFrameMargins() const {
    return d->emulatedFrameMargins;
}

void QnDwm::setEmulatedTitleBarHeight(int height) {
    d->emulatedTitleBarHeight = height;
}

int QnDwm::emulatedTitleBarHeight() const {
    return d->emulatedTitleBarHeight;
}

bool QnDwm::enableTitleBarDrag(bool enable) {
    if(!d->hasApi)
        return false;

#ifdef Q_OS_WIN
    d->titleBarDrag = enable;
    return true;
#else 
    return false;
#endif
}

QMargins QnDwm::currentFrameMargins() const {
    QMargins errorValue(-1, -1, -1, -1);

    if(!d->hasApi)
        return errorValue;

#ifdef Q_OS_WIN
    HWND hwnd = d->widget->winId();
    BOOL status = S_OK;

    RECT clientRect;
    status = GetClientRect(d->widget->winId(), &clientRect);
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
#endif
}

bool QnDwm::setCurrentFrameMargins(const QMargins &margins) {
    if(!d->hasApi)
        return false;

#ifdef Q_OS_WIN
    HWND hwnd = d->widget->winId();
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
    return false;
#endif
}

bool QnDwm::widgetEvent(QEvent *event) {
    Q_UNUSED(event);

    if(!d->hasApi)
        return false;

#ifdef Q_OS_WIN
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

        if(event->type() == QnInvocationEvent::Invocation && static_cast<QnInvocationEvent *>(event)->id() == AdjustPositionInvocation)
            d->widget->move(d->widget->pos()); 
    }
#endif

    return false;
}

#ifdef Q_OS_WIN
bool QnDwm::widgetWinEvent(MSG *message, long *result) {
    if(d->widget == NULL)
        return false;

    if(d->hasDwm) {
        RESULT localResult;
        BOOL handled = d->dwmDefWindowProc(message->hwnd, message->message, message->wParam, message->lParam, &localResult);
        if (handled) {
            *result = localResult;
            return true;
        }
    }

    switch(message->message) {
    case WM_NCCALCSIZE:             return d->calcSizeEvent(message, result);
    case WM_NCHITTEST:              return d->hitTestEvent(message, result);
    case WM_DWMCOMPOSITIONCHANGED:  return d->compositionChangedEvent(message, result);
    case WM_NCACTIVATE:             return d->activateEvent(message, result);
    case WM_NCPAINT:                return d->ncPaintEvent(message, result);
    case WM_SYSCOMMAND:             return d->sysCommandEvent(message, result);
    case WM_ERASEBKGND:             return d->eraseBackground(message, result);
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

namespace {
    int sweep(int v, int v0, int v1, int v2, int v3, int r0, int r1, int r2, int r3, int r4) {
        /* Note that [v0, v3] and [v1, v2] are treated as closed segments. */

        if(v <= v2) {
            if(v < v1) {
                if (v < v0) {
                    return r0;
                } else {
                    return r1;
                }
            } else {
                return r2;
            }
        } else if(v <= v3) {
            return r3;
        } else {
            return r4;
        }
    }

    int frameSectionMap[5][5] = {
        {HTNOWHERE,     HTNOWHERE,      HTNOWHERE,      HTNOWHERE,      HTNOWHERE},
        {HTNOWHERE,     HTTOPLEFT,      HTTOP,          HTTOPRIGHT,     HTNOWHERE},
        {HTNOWHERE,     HTLEFT,         HTCLIENT,       HTRIGHT,        HTNOWHERE},
        {HTNOWHERE,     HTBOTTOMLEFT,   HTBOTTOM,       HTBOTTOMRIGHT,  HTNOWHERE},
        {HTNOWHERE,     HTNOWHERE,      HTNOWHERE,      HTNOWHERE,      HTNOWHERE}
    };

} // anonymous namespace

bool QnDwmPrivate::hitTestEvent(MSG *message, long *result) {
    BOOL handled = FALSE;
    if(hasDwm) {
        RESULT localResult;
        handled = dwmDefWindowProc(message->hwnd, message->message, message->wParam, message->lParam, &localResult);
        if(handled)
            *result = localResult;
    }
    if(!handled)
        *result = DefWindowProc(message->hwnd, message->message, message->wParam, message->lParam);

    if(!emulateFrame)    
        return true;

    /* Leave buttons as is. */
    switch(*result) {
    case HTMINBUTTON:
    case HTMAXBUTTON:
    case HTCLOSE:
        return true;
    default:
        break;
    }

    BOOL status = S_OK;

    /* There is a bug in Qt: coordinates for WM_NCHITTEST are supplied in screen coordinates, 
     * but Qt calls ClientToScreen for them. This is why we re-extract screen coordinates from lParam. */
    POINT systemPos;
    systemPos.x = GET_X_LPARAM(message->lParam);
    systemPos.y = GET_Y_LPARAM(message->lParam);
    QPoint pos = QPoint(systemPos.x, systemPos.y);

    /* Get frame rect from the system. */
    RECT systemRect;
    status = GetWindowRect(message->hwnd, &systemRect);
    if(!SUCCEEDED(status))
        return false;

    QRect frameRect = QRect(QPoint(systemRect.left, systemRect.top), QPoint(systemRect.right, systemRect.bottom));
    
    /* Calculate emulated client rect, title bar excluded. */
    QRect clientRect = QRect(
        QPoint(
            frameRect.left() + emulatedFrameMargins.left(),
            frameRect.top() + emulatedFrameMargins.top()
        ),
        QPoint(
            frameRect.right() - emulatedFrameMargins.right(),
            frameRect.bottom() - emulatedFrameMargins.bottom()
        )
    );

    /* Find window frame section. */
    int row = sweep(pos.x(), frameRect.left(), clientRect.left(), clientRect.right(),  frameRect.right(),  0, 1, 2, 3, 4);
    int col = sweep(pos.y(), frameRect.top(),  clientRect.top(),  clientRect.bottom(), frameRect.bottom(), 0, 1, 2, 3, 4);
    *result = frameSectionMap[col][row];

    /* Handle title bar. */
    if(*result == HTCLIENT && pos.y() <= clientRect.top() + emulatedTitleBarHeight)
        *result = HTCAPTION;

    return true;
}

bool QnDwmPrivate::compositionChangedEvent(MSG *message, long *result) {
    Q_UNUSED(message);

    emit q->compositionChanged(q->isCompositionEnabled());

    *result = 0;
    return false; /* It's OK to let it fall through. */
}

bool QnDwmPrivate::activateEvent(MSG *message, long *result) {
    if(!systemPaintWindow)
        message->lParam = -1; /* Don't repaint the frame in default handler. It causes frame flickering. */

    *result = DefWindowProc(message->hwnd, message->message, message->wParam, message->lParam);
    return true;
}

bool QnDwmPrivate::ncPaintEvent(MSG *message, long *result) {
    if(!systemPaintWindow) {

        if(transparentErasing) {
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

            if(!nonErasableContentMargins.isNull()) {
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

                /* Adjust for non-erasable margins. */
                rect.left   += nonErasableContentMargins.left();
                rect.top    += nonErasableContentMargins.top();
                rect.right  -= nonErasableContentMargins.right();
                rect.bottom -= nonErasableContentMargins.bottom();

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
            }

            HBRUSH hbr = (HBRUSH) GetStockObject(BLACK_BRUSH);
            SelectBrush(hdc, hbr);
            PaintRgn(hdc, hrgn);
            DeleteObject(hbr);

            if(deleteHrgn)
                DeleteObject(hrgn);

            ReleaseDC(message->hwnd, hdc);
        }

        /* An application should return zero if it processes this message. */
        *result = 0;
        return true;
    } else {
        return false;
    }
}

bool QnDwmPrivate::eraseBackground(MSG *message, long *result) {
    if(!systemPaintWindow) {
        if(transparentErasing) {
            HDC hdc = (HDC) message->wParam;
            HBRUSH hbr = (HBRUSH) GetStockObject(BLACK_BRUSH);
            if(!hbr) {
                *result = 0;
                return true;
            }

            RECT rect;
            GetClientRect(message->hwnd, &rect);
            DPtoLP(hdc, (LPPOINT) &rect, 2);
            FillRect(hdc, &rect, hbr);
        }

        /* An application should return non-zero if it processes this message. */
        *result = 1;
        return true;
    } else {
        return false;
    }
}

bool QnDwmPrivate::sysCommandEvent(MSG *message, long *result) {
    switch(message->wParam & 0xFFF0) {
    case SC_MOVE:
        if(titleBarDrag) {
            return false;
        } else {
            *result = 0; 
            return true; 
        }
    default:
        return false;
    }
}

#endif

