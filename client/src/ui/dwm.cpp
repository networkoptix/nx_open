#include "dwm.h"
#include <QLibrary>
#include <QWidget>
#include <qt_windows.h>
#include <utils/common/warnings.h>

#ifdef Q_OS_WIN

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

typedef HRESULT (WINAPI *PtrDwmIsCompositionEnabled)(BOOL* pfEnabled);
typedef HRESULT (WINAPI *PtrDwmExtendFrameIntoClientArea)(HWND hWnd, const _MARGINS *pMarInset);
typedef HRESULT (WINAPI *PtrDwmEnableBlurBehindWindow)(HWND hWnd, const _DWM_BLURBEHIND *pBlurBehind);
typedef HRESULT (WINAPI *PtrDwmGetColorizationColor)(DWORD *pcrColorization, BOOL *pfOpaqueBlend);
typedef BOOL (WINAPI *PtrDwmDefWindowProc)(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *plResult);

#endif


class QnDwmPrivate {
public:
    QnDwmPrivate() {}

    virtual ~QnDwmPrivate() {}

    void init(QWidget *widget);

public:
    /** Widget that we're working on. */
    QWidget *widget;

#ifdef Q_OS_WIN
    /** Whether DWM is available. */
    bool hasDwm;

    /* Function pointers for functions from dwmapi.dll. */
    PtrDwmIsCompositionEnabled dwmIsCompositionEnabled;
    PtrDwmEnableBlurBehindWindow dwmEnableBlurBehindWindow;
    PtrDwmExtendFrameIntoClientArea dwmExtendFrameIntoClientArea;
    PtrDwmGetColorizationColor dwmGetColorizationColor;
    PtrDwmDefWindowProc dwmDefWindowProc;
    
    /** Widget frame margins specified by the user. */
    QMargins userFrameMargins;

    bool overrideFrameMargins;
    bool systemPaintFrame;
    bool processDoubleClicks;
    bool titleBarDrag;
#endif

    bool emulateFrame;
    QMargins emulatedFrameMargins;
    int emulatedTitleBarHeight;
};

void QnDwmPrivate::init(QWidget *widget) {
    this->widget = widget;

#ifdef Q_OS_WIN
    QLibrary dwmLib(QString::fromAscii("dwmapi"));
    dwmIsCompositionEnabled         = (PtrDwmIsCompositionEnabled)      dwmLib.resolve("DwmIsCompositionEnabled");
    dwmExtendFrameIntoClientArea    = (PtrDwmExtendFrameIntoClientArea) dwmLib.resolve("DwmExtendFrameIntoClientArea");
    dwmEnableBlurBehindWindow       = (PtrDwmEnableBlurBehindWindow)    dwmLib.resolve("DwmEnableBlurBehindWindow");
    dwmGetColorizationColor         = (PtrDwmGetColorizationColor)      dwmLib.resolve("DwmGetColorizationColor");
    dwmDefWindowProc                = (PtrDwmDefWindowProc)             dwmLib.resolve("DwmDefWindowProc");

    hasDwm = 
        dwmIsCompositionEnabled != NULL && 
        dwmExtendFrameIntoClientArea != NULL &&
        dwmEnableBlurBehindWindow != NULL &&
        dwmGetColorizationColor != NULL &&
        dwmDefWindowProc != NULL;

    overrideFrameMargins = false;
    systemPaintFrame = true;
    processDoubleClicks = false;
    titleBarDrag = true;
#endif

    emulateFrame = false;
    emulatedTitleBarHeight = 0;
}

QnDwm::QnDwm(QWidget *widget):
    QObject(widget),
    d(new QnDwmPrivate())
{
    if(widget == NULL)
        qnNullWarning(widget);

    d->init(widget);
}

QnDwm::~QnDwm() {
    delete d;
    d = NULL;
}

bool QnDwm::isSupported() const {
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

bool QnDwm::enableSystemFramePainting(bool enable) {
    if(d->widget == NULL)
        return false;

#ifdef Q_OS_WIN
    d->systemPaintFrame = enable;
    return true;
#else
    return false;
#endif
}

bool QnDwm::enableBlurBehindWindow(bool enable) {
    if(d->widget == NULL)
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
    if(d->widget == NULL)
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
        
    return SUCCEEDED(status);
#else
    return false;
#endif
}

QMargins QnDwm::themeFrameMargins() const {
    if(d->widget == NULL)
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
    if(d->widget == NULL)
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
    if(d->widget == NULL)
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

void QnDwm::enableDoubleClickProcessing(bool enable) {
    d->processDoubleClicks = enable;
}

void QnDwm::enableTitleBarDrag(bool enable) {
    d->titleBarDrag = enable;
}

QMargins QnDwm::currentFrameMargins() const {
    QMargins errorValue(-1, -1, -1, -1);

    if(d->widget == NULL)
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
    if(d->widget == NULL)
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

    return true;
#else
    return false;
#endif
}

#ifdef Q_OS_WIN
bool QnDwm::winEvent(MSG *message, long *result) {
    if(d->widget == NULL)
        return false;

    if(!d->hasDwm)
        return false;

    BOOL handled = d->dwmDefWindowProc(message->hwnd, message->message, message->wParam, message->lParam, result);
    if (handled)
        return true;

    switch(message->message) {
    case WM_NCCALCSIZE:             return calcSizeEvent(message, result);
    case WM_NCHITTEST:              return hitTestEvent(message, result);
    case WM_DWMCOMPOSITIONCHANGED:  return compositionChangedEvent(message, result);
    case WM_NCACTIVATE:             return activateEvent(message, result);
    case WM_NCPAINT:                return ncPaintEvent(message, result);
    case WM_NCLBUTTONDBLCLK:        return ncLeftButtonDoubleClickEvent(message, result);
    case WM_SYSCOMMAND:             return sysCommandEvent(message, result);
    default:                        return false;
    }
}

bool QnDwm::calcSizeEvent(MSG *message, long *result) {
    if(!d->overrideFrameMargins)
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
    /* RECT sourceClientGeometry = params->rgrc[2]; */

    /* Prepare output.
     * 
     * 0 - the coordinates of the new client rectangle resulting from the move or resize.
     * 1 - the valid destination rectangle.
     * 2 - the valid source rectangle. */
    params->rgrc[1] = targetGeometry;
    params->rgrc[2] = sourceGeometry;

    /* New client geometry. */
    params->rgrc[0].top      += d->userFrameMargins.top();
    params->rgrc[0].bottom   -= d->userFrameMargins.bottom();
    params->rgrc[0].left     += d->userFrameMargins.left();
    params->rgrc[0].right    -= d->userFrameMargins.right();

    //*result = WVR_VALIDRECTS;
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

bool QnDwm::hitTestEvent(MSG *message, long *result) {
    BOOL handled = d->dwmDefWindowProc(message->hwnd, message->message, message->wParam, message->lParam, result);
    if(!handled)
        *result = DefWindowProc(message->hwnd, message->message, message->wParam, message->lParam);

    if(!d->emulateFrame)    
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
            frameRect.left() + d->emulatedFrameMargins.left(),
            frameRect.top() + d->emulatedFrameMargins.top()
        ),
        QPoint(
            frameRect.right() - d->emulatedFrameMargins.right(),
            frameRect.bottom() - d->emulatedFrameMargins.bottom()
        )
    );

    /* Find window frame section. */
    int row = sweep(pos.x(), frameRect.left(), clientRect.left(), clientRect.right(),  frameRect.right(),  0, 1, 2, 3, 4);
    int col = sweep(pos.y(), frameRect.top(),  clientRect.top(),  clientRect.bottom(), frameRect.bottom(), 0, 1, 2, 3, 4);
    *result = frameSectionMap[col][row];

    /* Handle title bar. */
    if(*result == HTCLIENT && pos.y() <= clientRect.top() + d->emulatedTitleBarHeight)
        *result = HTCAPTION;

    return true;
}

bool QnDwm::compositionChangedEvent(MSG *message, long *result) {
    Q_UNUSED(message);

    emit compositionChanged(isCompositionEnabled());

    *result = 0;
    return false; /* It's OK to let it fall through. */
}

bool QnDwm::activateEvent(MSG *message, long *result) {
    if(!d->systemPaintFrame)
        message->lParam = -1; /* Don't repaint the frame in default handler. It causes frame flickering. */

    *result = DefWindowProc(message->hwnd, message->message, message->wParam, message->lParam);
    return true;
}

bool QnDwm::ncPaintEvent(MSG *message, long *result) {
    if(!d->systemPaintFrame) {
        /* An application should return zero if it processes this message. */
        *result = 0;
        return true;
    } else {
        return false;
    }
}

bool QnDwm::ncLeftButtonDoubleClickEvent(MSG *message, long *result) {
    if(!d->processDoubleClicks)    
        return false;

    if(message->wParam == HTCAPTION) {
        emit titleBarDoubleClicked();
        return true;
    }

    return false;
}

bool QnDwm::sysCommandEvent(MSG *message, long *result) {
    switch(message->wParam & 0xFFF0) {
    case SC_MOVE:
        if(d->titleBarDrag) {
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

