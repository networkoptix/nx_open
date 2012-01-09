#include "dmw.h"
#include <QLibrary>
#include <QWidget>
#include <qt_windows.h>
#include <utils/common/warnings.h>

#ifdef Q_OS_WIN
#    define NOMINMAX
#    include <Windows.h>
#    include <WindowsX.h>
#endif

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

class QnDwmPrivate {
public:
    QnDwmPrivate();

    virtual ~QnDwmPrivate() {}

    void init(QWidget *widget);

    void updateThemeMetrics();

public:
    bool valid;
    QWidget *widget;
    PtrDwmIsCompositionEnabled dwmIsCompositionEnabled;
    PtrDwmEnableBlurBehindWindow dwmEnableBlurBehindWindow;
    PtrDwmExtendFrameIntoClientArea dwmExtendFrameIntoClientArea;
    PtrDwmGetColorizationColor dwmGetColorizationColor;
    PtrDwmDefWindowProc dwmDefWindowProc;
    QMargins widgetFrameMargins;
    int widgetTitleBarHeight;
    QMargins themeFrameMargins;
    int themeTitleBarHeight;
};

QnDwmPrivate::QnDwmPrivate() { 
    memset(this, 0, sizeof(this)); 
}

void QnDwmPrivate::init(QWidget *widget) {
    this->widget = widget;

#ifdef Q_OS_WIN
    QLibrary dwmLib(QString::fromAscii("dwmapi"));
    dwmIsCompositionEnabled         = (PtrDwmIsCompositionEnabled)      dwmLib.resolve("DwmIsCompositionEnabled");
    dwmExtendFrameIntoClientArea    = (PtrDwmExtendFrameIntoClientArea) dwmLib.resolve("DwmExtendFrameIntoClientArea");
    dwmEnableBlurBehindWindow       = (PtrDwmEnableBlurBehindWindow)    dwmLib.resolve("DwmEnableBlurBehindWindow");
    dwmGetColorizationColor         = (PtrDwmGetColorizationColor)      dwmLib.resolve("DwmGetColorizationColor");
    dwmDefWindowProc                = (PtrDwmDefWindowProc)             dwmLib.resolve("DwmDefWindowProc");
#endif

    valid = dwmIsCompositionEnabled != NULL;

    updateThemeMetrics();

    widgetTitleBarHeight = themeTitleBarHeight;
    widgetFrameMargins = themeFrameMargins;
}

void QnDwmPrivate::updateThemeMetrics() {
    if(!valid)
        return;

#ifdef Q_OS_WIN
    int frameX, frameY;
    if(widget->windowFlags() & Qt::FramelessWindowHint || widget->windowFlags() & Qt::X11BypassWindowManagerHint) {
        frameX = 0;
        frameY = 0;
        themeTitleBarHeight = 0;
    } else if(widget->windowFlags() & Qt::MSWindowsFixedSizeDialogHint) {
        frameX = GetSystemMetrics(SM_CXFIXEDFRAME);
        frameY = GetSystemMetrics(SM_CYFIXEDFRAME);
        themeTitleBarHeight = GetSystemMetrics(SM_CYSMCAPTION);
    } else {
        frameX = GetSystemMetrics(SM_CXSIZEFRAME);
        frameY = GetSystemMetrics(SM_CYSIZEFRAME);
        themeTitleBarHeight = GetSystemMetrics(SM_CYCAPTION);
    }

    themeFrameMargins.setTop(frameY);
    themeFrameMargins.setBottom(frameY);
    themeFrameMargins.setLeft(frameX);
    themeFrameMargins.setRight(frameY);
#else
    themeFrameMargins = QMargins(-1, -1, -1, -1);
    themeTitleBarHeight = -1;
#endif
}

QnDwm::QnDwm(QWidget *widget):
    QObject(widget),
    d(new QnDwmPrivate())
{
    if(widget == NULL) {
        qnNullWarning(widget);
        return;
    }

    d->init(widget);
}

QnDwm::~QnDwm() {
    delete d;
    d = NULL;
}

bool QnDwm::enableBlurBehindWindow(bool enable) {
    if(!d->valid)
        return false;

#ifdef Q_OS_WIN
    _DWM_BLURBEHIND blurBehind = {0};
    HRESULT status = S_OK;
    blurBehind.fEnable = enable;
    blurBehind.dwFlags = DWM_BB_ENABLE;
    blurBehind.hRgnBlur = NULL;
    
    status = d->dwmEnableBlurBehindWindow(d->widget->winId(), &blurBehind);
    
    if(SUCCEEDED(status)) {
        d->widget->setAttribute(Qt::WA_TranslucentBackground, enable);
        d->widget->setAttribute(Qt::WA_NoSystemBackground, enable);
        return true;
    } else {
        return false;
    }
#else
    return false;
#endif
}

bool QnDwm::isCompositionEnabled() {
    if (!d->valid)
        return false;
    
#ifdef Q_OS_WIN
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
    if(!d->valid)
        return false;

#ifdef Q_OS_WIN
    HRESULT status = S_OK;
    _MARGINS winMargins;
    winMargins.cxLeftWidth      = margins.left();
    winMargins.cxRightWidth     = margins.right();
    winMargins.cyBottomHeight   = margins.bottom();
    winMargins.cyTopHeight      = margins.top();
    
    status = d->dwmExtendFrameIntoClientArea(d->widget->winId(), &winMargins);
        
    if (SUCCEEDED(status)) {
        d->widget->setAttribute(Qt::WA_TranslucentBackground, true);
        return true;
    } else {
        return false;
    }
#else
    return false;
#endif
}

QMargins QnDwm::themeFrameMargins() {
    return d->themeFrameMargins;
}

int QnDwm::themeTitleBarHeight() {
    return d->themeTitleBarHeight;
}

QMargins QnDwm::systemFrameMargins() {
    if(!d->valid)
        goto error;

    HWND hwnd = d->widget->winId();
    BOOL status = S_OK;

    RECT clientRect;
    status = GetClientRect(d->widget->winId(), &clientRect);
    if(!SUCCEEDED(status))
        goto error;

    RECT windowRect;
    status = GetWindowRect(hwnd, &windowRect);
    if(!SUCCEEDED(status))
        goto error;

    POINT clientTopLeft = {0, 0};
    status = ClientToScreen(hwnd, &clientTopLeft);
    if(!SUCCEEDED(status))
        goto error;

    return QMargins(
        clientTopLeft.x - windowRect.left,
        clientTopLeft.y - windowRect.top,
        windowRect.right - clientTopLeft.x - clientRect.right,
        windowRect.bottom - clientTopLeft.y - clientRect.bottom
    );
error:
    return QMargins(-1, -1, -1, -1);
}

bool QnDwm::setSystemFrameMargins(const QMargins &margins) {
    if(!d->valid)
        return false;

    HWND hwnd = d->widget->winId();
    BOOL status = S_OK;

    //status = SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_DEFERERASE | )
}

bool QnDwm::winEvent(MSG *message, long *result) {
    if(!d->valid)
        return false;

#ifdef Q_OS_WIN
    BOOL handled = d->dwmDefWindowProc(message->hwnd, message->message, message->wParam, message->lParam, result);
    if (handled)
        return true;

    switch(message->message) {
    case WM_NCCALCSIZE:             return calcSizeEvent(message, result);
    case WM_NCHITTEST:              return hitTestEvent(message, result);
    case WM_DWMCOMPOSITIONCHANGED:  return compositionChangedEvent(message, result);
    default:                        return false;
    }
#else
    return false;
#endif
}

#ifdef Q_OS_WIN
bool QnDwm::calcSizeEvent(MSG *message, long *result) {
    /* From WM_NCCALCSIZE docs:
     *
     * If wParam is TRUE, it specifies that the application should indicate which 
     * part of the client area contains valid information. 
     * The system copies the valid information to the specified area within the new client area.
     *
     * If wParam is FALSE, the application does not need to indicate the valid part of the client area. */
    if(!message->wParam)
        return false;

    NCCALCSIZE_PARAMS *nccsp = (NCCALCSIZE_PARAMS *) message->lParam;

    /* Adjust (shrink) the client rectangle to accommodate the border. 
     * 
     * Don't include the title bar. */
    RECT targetGeometry = nccsp->rgrc[0];
    RECT sourceGeometry = nccsp->rgrc[1];
    RECT sourceClientGeometry = nccsp->rgrc[2];

    /* New client geometry. */
    nccsp->rgrc[0].top      += 0;
    nccsp->rgrc[0].bottom   += 0;
    nccsp->rgrc[0].left     += 0;
    nccsp->rgrc[0].right    += 0;
    //nccsp->rgrc[0].bottom   += sourceClientGeometry.bottom - sourceGeometry.bottom;
    //nccsp->rgrc[0].left     += sourceClientGeometry.left   - sourceGeometry.left;
    //nccsp->rgrc[0].right    += sourceClientGeometry.right  - sourceGeometry.right;

    /* New geometry. */
    nccsp->rgrc[1] = targetGeometry;
    
    /* Old geometry. */
    nccsp->rgrc[2] = sourceGeometry;


    *result = WVR_VALIDRECTS;
    return true;
}

bool QnDwm::hitTestEvent(MSG *message, long *result) {
    *result = DefWindowProc(message->hwnd, message->message, message->wParam, message->lParam);
        
    switch(*result) {
    case HTCLIENT:
    case HTMINBUTTON:
    case HTMAXBUTTON:
    case HTCLOSE:
        *result = HTCAPTION;
        break;
    case HTLEFT:
    case HTRIGHT:
    case HTTOPLEFT:
    case HTTOPRIGHT:
        return true;
    default:
        break;
    }

    /* There is a bug in Qt: coordinates for WM_NCHITTEST are supplied in screen coordinates, 
     * but Qt calls ClientToScreen for them. This is why we re-extract screen coordinates from lParam. */
    POINT localPos;
    localPos.x = GET_X_LPARAM(message->lParam);
    localPos.y = GET_Y_LPARAM(message->lParam);
    ScreenToClient(message->hwnd, &localPos);

    if(localPos.y > d->themeFrameMargins.top() + d->themeTitleBarHeight) {
        /* Do nothing. */
    } else if(localPos.y < d->themeFrameMargins.top()) {
        *result = HTTOP;
    } else {
        *result = HTCAPTION;
    }

    return true;
}

bool QnDwm::compositionChangedEvent(MSG *message, long *result) {
    Q_UNUSED(message);

    emit compositionChanged(isCompositionEnabled());

    *result = 0;
    return true;
}

#endif

