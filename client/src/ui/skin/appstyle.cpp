#include "appstyle_p.h"

#include <QtGui/QApplication>
#include <QtGui/QPainter>
#include <QtGui/QStyleOption>

#include "skin.h"

static const float global_dialog_opacity = 0.9f;
static const float global_menu_opacity = 0.8f;

AppStyle::AppStyle(const QString &baseStyle, QObject *parent)
    : ProxyStyle(baseStyle, parent)
{
}

void AppStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    switch (control) {
#ifndef QT_NO_SLIDER
    case CC_Slider:
        if (widget && widget->inherits("QAbstractSlider")) { // for old good widgets
            ProxyStyle::drawComplexControl(control, option, painter, widget);
            break;
        }

        // ### optimize in Bespin & remove
        if (const QStyleOptionSlider *sliderOption = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            if (sliderOption->orientation != Qt::Horizontal) {
                qWarning("AppStyle: Non-horizontal sliders are not implemented. Falling back to the default painting.");
                ProxyStyle::drawComplexControl(control, option, painter, widget);
                break;
            }

            const QRect grooveRect = subControlRect(CC_Slider, option, SC_SliderGroove, widget);
            QRect handleRect = subControlRect(CC_Slider, option, SC_SliderHandle, widget);
            handleRect.setSize(handleRect.size() * 2);
            handleRect.moveTopLeft(handleRect.topLeft() - QPoint(handleRect.width(), handleRect.height()) / 4);

            const bool hovered = (option->state & State_Enabled) && (option->activeSubControls & SC_SliderHandle);

            QPixmap grooveBorderPic = Skin::pixmap(QLatin1String("slider_groove_lborder.png"), grooveRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QPixmap grooveBodyPic = Skin::pixmap(QLatin1String("slider_groove_body.png"), grooveRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QPixmap handlePic = Skin::pixmap(hovered ? QLatin1String("slider_handle_active.png") : QLatin1String("slider_handle.png"), handleRect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

            painter->drawTiledPixmap(grooveRect.adjusted(grooveBorderPic.width(), 0, -grooveBorderPic.width(), 0), grooveBodyPic);
            painter->drawPixmap(grooveRect.topLeft(), grooveBorderPic);
            {
                QTransform oldTransform = painter->transform();
                painter->translate(grooveRect.left() + grooveRect.width(), grooveRect.top());
                painter->scale(-1.0, 1.0);
                painter->drawPixmap(0, 0, grooveBorderPic);
                painter->setTransform(oldTransform);
            }

            painter->drawPixmap(handleRect, handlePic);
        }
        break;
#endif // QT_NO_SLIDER
    case CC_TitleBar:
        {
            // draw background in order to avoid painting artifacts
            QColor bgColor = option->palette.color(widget ? widget->backgroundRole() : QPalette::Window);
            if (bgColor != Qt::transparent) {
                bgColor.setAlpha(255);
                painter->fillRect(option->rect, bgColor);
            }
        }
        // fall through

    default:
        ProxyStyle::drawComplexControl(control, option, painter, widget);
    }
}

int AppStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const
{
    if (hint == QStyle::SH_ToolTipLabel_Opacity)
        return 255;

    return ProxyStyle::styleHint(hint, option, widget, returnData);
}

void AppStyle::polish(QApplication *application)
{
    ProxyStyle::polish(application);

    QFont menuFont;
    menuFont.setFamily(QLatin1String("Bodoni MT"));
    menuFont.setPixelSize(18);
    application->setFont(menuFont, "QMenu");
}

void AppStyle::unpolish(QApplication *application)
{
    application->setFont(QFont(), "QMenu");

    ProxyStyle::unpolish(application);
}

void AppStyle::polish(QWidget *widget)
{
    ProxyStyle::polish(widget);

/*    if (widget->inherits("QDialog"))
        widget->setWindowOpacity(global_dialog_opacity);
    else if (widget->inherits("QMenu"))
        widget->setWindowOpacity(global_menu_opacity);*/
}

void AppStyle::unpolish(QWidget *widget)
{
/*    if (widget->inherits("QDialog"))
        widget->setWindowOpacity(1.0);
    else if (widget->inherits("QMenu"))
        widget->setWindowOpacity(1.0);*/

    ProxyStyle::unpolish(widget);
}


AppProxyStyle::AppProxyStyle(QStyle *style)
    : QProxyStyle(style)
{
}

static inline bool buttonVisible(const QStyle::SubControl sc, const QStyleOptionTitleBar *tb)
{
    const bool isMinimized = tb->titleBarState & Qt::WindowMinimized;
    const bool isMaximized = tb->titleBarState & Qt::WindowMaximized;
    const bool isFullScreen = tb->titleBarState & Qt::WindowFullScreen;
    const uint flags = tb->titleBarFlags;

    switch (sc) {
    case QStyle::SC_TitleBarSysMenu: return (flags & Qt::WindowSystemMenuHint);
    case QStyle::SC_TitleBarMinButton: return (!isMinimized && (flags & Qt::WindowMinimizeButtonHint));
    case QStyle::SC_TitleBarMaxButton: return (!isMaximized && (flags & Qt::WindowMaximizeButtonHint));
    case QStyle::SC_TitleBarCloseButton: return (flags & Qt::WindowSystemMenuHint);
    case QStyle::SC_TitleBarNormalButton: return (isMinimized && (flags & Qt::WindowMinimizeButtonHint)) ||
                                                 (isMaximized && (flags & Qt::WindowMaximizeButtonHint));
    case QStyle::SC_TitleBarShadeButton: return (!isFullScreen && (flags & Qt::WindowShadeButtonHint));
    case QStyle::SC_TitleBarUnshadeButton: return (isFullScreen && (flags & Qt::WindowShadeButtonHint));
    case QStyle::SC_TitleBarContextHelpButton: return (flags & Qt::WindowContextHelpButtonHint);
    case QStyle::SC_TitleBarLabel: return true;
    default: break;
    }

    return false;
}

QRect AppProxyStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const
{
    QRect rect;

    switch (control) {
    case CC_TitleBar:
        if (const QStyleOptionTitleBar *tb = qstyleoption_cast<const QStyleOptionTitleBar *>(option)) {
            if (!buttonVisible(subControl, tb))
                return rect;

            const bool isMinimized = tb->titleBarState & Qt::WindowMinimized;
            const bool isMaximized = tb->titleBarState & Qt::WindowMaximized;
            const bool isFullScreen = tb->titleBarState & Qt::WindowFullScreen;

            const int frameWidth = pixelMetric(PM_MdiSubWindowFrameWidth, option, widget);
            const int controlMargin = 2;
            const int controlHeight = tb->rect.height() - controlMargin * 2;
            const int delta = controlHeight + controlMargin * 2;
            int offset = 0;

            switch (subControl) {
            case SC_TitleBarLabel:
                if (tb->titleBarFlags & (Qt::WindowTitleHint | Qt::WindowSystemMenuHint)) {
                    rect = tb->rect.adjusted(frameWidth, 0, -frameWidth, 0);
                    if (tb->titleBarFlags & Qt::WindowSystemMenuHint)
                        rect.adjust(delta, 0, -delta, 0);
                    if (tb->titleBarFlags & Qt::WindowContextHelpButtonHint)
                        rect.adjust(0, 0, -delta, 0);
                    if (tb->titleBarFlags & Qt::WindowShadeButtonHint)
                        rect.adjust(0, 0, -delta, 0);
                    if (tb->titleBarFlags & Qt::WindowMinimizeButtonHint)
                        rect.adjust(0, 0, -delta, 0);
                    if (tb->titleBarFlags & Qt::WindowMaximizeButtonHint)
                        rect.adjust(0, 0, -delta, 0);
                }
                break;
            case SC_TitleBarContextHelpButton:
                if (tb->titleBarFlags & Qt::WindowContextHelpButtonHint)
                    offset += delta;
                // fall through
            case SC_TitleBarShadeButton:
                if (!isFullScreen && (tb->titleBarFlags & Qt::WindowShadeButtonHint))
                    offset += delta;
                else if (subControl == SC_TitleBarShadeButton)
                    break;
                // fall through
            case SC_TitleBarUnshadeButton:
                if (isFullScreen && (tb->titleBarFlags & Qt::WindowShadeButtonHint))
                    offset += delta;
                else if (subControl == SC_TitleBarUnshadeButton)
                    break;
                // fall through
            case SC_TitleBarMinButton:
                if (!isMinimized && (tb->titleBarFlags & Qt::WindowMinimizeButtonHint))
                    offset += delta;
                else if (subControl == SC_TitleBarMinButton)
                    break;
                // fall through
            case SC_TitleBarNormalButton:
                if (isMinimized && (tb->titleBarFlags & Qt::WindowMinimizeButtonHint))
                    offset += delta;
                else if (isMaximized && (tb->titleBarFlags & Qt::WindowMaximizeButtonHint))
                    offset += delta;
                else if (subControl == SC_TitleBarNormalButton)
                    break;
                //fall through
            case SC_TitleBarMaxButton:
                if (!isMaximized && (tb->titleBarFlags & Qt::WindowMaximizeButtonHint))
                    offset += delta;
                else if (subControl == SC_TitleBarMaxButton)
                    break;
                // fall through
            case SC_TitleBarCloseButton:
                if (tb->titleBarFlags & Qt::WindowSystemMenuHint)
                    offset += delta;
                else if (subControl == SC_TitleBarCloseButton)
                    break;
                rect.setRect(tb->rect.right() - offset, tb->rect.top() + controlMargin,
                             controlHeight, controlHeight);
                break;
            case SC_TitleBarSysMenu:
                if (tb->titleBarFlags & Qt::WindowSystemMenuHint) {
                    rect.setRect(tb->rect.left() + frameWidth, tb->rect.top() + controlMargin,
                                 controlHeight, controlHeight);
                }
                break;

            default:
                break;
            }
            rect = visualRect(tb->direction, tb->rect, rect);
        }
        break;

    default:
        rect = QProxyStyle::subControlRect(control, option, subControl, widget);
        break;
    }

    return rect;
}

QPixmap AppProxyStyle::standardPixmap(StandardPixmap standardPixmap, const QStyleOption *option, const QWidget *widget) const
{
    QPixmap pm;

    switch (standardPixmap) {
    case SP_TitleBarMinButton:
        pm = Skin::pixmap(QLatin1String("decorations/minimize.png"));
        break;
//    case SP_TitleBarMaxButton: // ###
//        pm = Skin::pixmap(QLatin1String("decorations/maximize.png"));
//        break;
    case SP_TitleBarCloseButton:
        pm = Skin::pixmap(QLatin1String("decorations/exit-application.png"));
        break;
    case SP_TitleBarNormalButton:
        pm = Skin::pixmap(QLatin1String("decorations/restore.png"));
        break;
    case SP_TitleBarShadeButton:
        pm = Skin::pixmap(QLatin1String("decorations/fullscreen.png"));
        break;
    case SP_TitleBarUnshadeButton:
        pm = Skin::pixmap(QLatin1String("decorations/unfullscreen.png"));
        break;
//    case SP_TitleBarContextHelpButton: // ###
//        pm = Skin::pixmap(QLatin1String("decorations/help.png"));
//        break;
    case SP_ArrowLeft:
        pm = Skin::pixmap(QLatin1String("left-arrow.png"));
        break;
    case SP_ArrowRight:
        pm = Skin::pixmap(QLatin1String("right-arrow.png"));
        break;
    case SP_ArrowUp:
        pm = Skin::pixmap(QLatin1String("up.png"));
        break;
    case SP_ArrowDown:
        pm = Skin::pixmap(QLatin1String("down.png"));
        break;
    case SP_BrowserReload:
        pm = Skin::pixmap(QLatin1String("refresh.png"));
        break;
    case SP_BrowserStop:
        pm = Skin::pixmap(QLatin1String("stop.png"));
        break;
/*    case SP_MediaPlay:
        pm = Skin::pixmap(QLatin1String("play_grey.png"));
        break;
    case SP_MediaPause:
        pm = Skin::pixmap(QLatin1String("pause_grey.png"));
        break;
//    case SP_MediaStop:
//        pm = Skin::pixmap(QLatin1String("stop_grey.png"));
//        break;
    case SP_MediaSeekForward:
        pm = Skin::pixmap(QLatin1String("forward_grey.png"));
        break;
    case SP_MediaSeekBackward:
        pm = Skin::pixmap(QLatin1String("backward_grey.png"));
        break;
    case SP_MediaSkipForward:
        pm = Skin::pixmap(QLatin1String("rewind_forward_grey.png"));
        break;
    case SP_MediaSkipBackward:
        pm = Skin::pixmap(QLatin1String("rewind_backward_grey.png"));
        break;*/
    case SP_MediaVolume:
        pm = Skin::pixmap(QLatin1String("unmute.png"));
        break;
    case SP_MediaVolumeMuted:
        pm = Skin::pixmap(QLatin1String("mute.png"));
        break;

    default:
        pm = QProxyStyle::standardPixmap(standardPixmap, option, widget);
        break;
    }

    return pm;
}

QIcon AppProxyStyle::standardIconImplementation(StandardPixmap standardIcon, const QStyleOption *option, const QWidget *widget) const
{
    QIcon icon;

    switch (standardIcon) {
    case SP_TitleBarMinButton:
        icon.addPixmap(Skin::pixmap(QLatin1String("decorations/minimize.png"), QSize(16, 16), Qt::KeepAspectRatio));
        break;
//    case SP_TitleBarMaxButton: // ###
//        icon.addPixmap(Skin::pixmap(QLatin1String("decorations/maximize.png"), QSize(16, 16), Qt::KeepAspectRatio));
//        break;
    case SP_TitleBarCloseButton:
        icon.addPixmap(Skin::pixmap(QLatin1String("decorations/exit-application.png"), QSize(16, 16), Qt::KeepAspectRatio));
        break;
    case SP_TitleBarNormalButton:
        icon.addPixmap(Skin::pixmap(QLatin1String("decorations/restore.png"), QSize(16, 16), Qt::KeepAspectRatio));
        break;
    case SP_TitleBarShadeButton:
        icon.addPixmap(Skin::pixmap(QLatin1String("decorations/fullscreen.png"), QSize(16, 16), Qt::KeepAspectRatio));
        break;
    case SP_TitleBarUnshadeButton:
        icon.addPixmap(Skin::pixmap(QLatin1String("decorations/unfullscreen.png"), QSize(16, 16), Qt::KeepAspectRatio));
        break;
//    case SP_TitleBarContextHelpButton: // ###
//        icon.addPixmap(Skin::pixmap(QLatin1String("decorations/help.png"), QSize(16, 16), Qt::KeepAspectRatio));
//        break;
    case SP_ArrowLeft:
        icon.addPixmap(Skin::pixmap(QLatin1String("left-arrow.png"), QSize(16, 16), Qt::KeepAspectRatio));
        icon.addPixmap(Skin::pixmap(QLatin1String("left-arrow.png"), QSize(24, 24), Qt::KeepAspectRatio));
        icon.addPixmap(Skin::pixmap(QLatin1String("left-arrow.png"), QSize(32, 32), Qt::KeepAspectRatio));
        break;
    case SP_ArrowRight:
        icon.addPixmap(Skin::pixmap(QLatin1String("right-arrow.png"), QSize(16, 16), Qt::KeepAspectRatio));
        icon.addPixmap(Skin::pixmap(QLatin1String("right-arrow.png"), QSize(24, 24), Qt::KeepAspectRatio));
        icon.addPixmap(Skin::pixmap(QLatin1String("right-arrow.png"), QSize(32, 32), Qt::KeepAspectRatio));
        break;
    case SP_ArrowUp:
        icon.addPixmap(Skin::pixmap(QLatin1String("up.png"), QSize(16, 16), Qt::KeepAspectRatio));
        icon.addPixmap(Skin::pixmap(QLatin1String("up.png"), QSize(24, 24), Qt::KeepAspectRatio));
        icon.addPixmap(Skin::pixmap(QLatin1String("up.png"), QSize(32, 32), Qt::KeepAspectRatio));
        break;
    case SP_ArrowDown:
        icon.addPixmap(Skin::pixmap(QLatin1String("down.png"), QSize(16, 16), Qt::KeepAspectRatio));
        icon.addPixmap(Skin::pixmap(QLatin1String("down.png"), QSize(24, 24), Qt::KeepAspectRatio));
        icon.addPixmap(Skin::pixmap(QLatin1String("down.png"), QSize(32, 32), Qt::KeepAspectRatio));
        break;
    case SP_BrowserReload:
        icon.addPixmap(Skin::pixmap(QLatin1String("refresh.png"), QSize(16, 16), Qt::KeepAspectRatio));
        icon.addPixmap(Skin::pixmap(QLatin1String("refresh.png"), QSize(24, 24), Qt::KeepAspectRatio));
        break;
    case SP_BrowserStop:
        icon.addPixmap(Skin::pixmap(QLatin1String("stop.png"), QSize(16, 16), Qt::KeepAspectRatio));
        icon.addPixmap(Skin::pixmap(QLatin1String("stop.png"), QSize(24, 24), Qt::KeepAspectRatio));
        break;
/*    case SP_MediaPlay:
        icon.addPixmap(Skin::pixmap(QLatin1String("play_grey.png"), QSize(16, 16), Qt::KeepAspectRatio));
        icon.addPixmap(Skin::pixmap(QLatin1String("play_blue.png"), QSize(16, 16), Qt::KeepAspectRatio), QIcon::Active);
        break;
    case SP_MediaPause:
        icon.addPixmap(Skin::pixmap(QLatin1String("pause_grey.png"), QSize(16, 16), Qt::KeepAspectRatio));
        icon.addPixmap(Skin::pixmap(QLatin1String("pause_blue.png"), QSize(16, 16), Qt::KeepAspectRatio), QIcon::Active);
        break;
//    case SP_MediaStop:
//        icon.addPixmap(Skin::pixmap(QLatin1String("stop_grey.png"), QSize(16, 16), Qt::KeepAspectRatio));
//        icon.addPixmap(Skin::pixmap(QLatin1String("stop_blue.png"), QSize(16, 16), Qt::KeepAspectRatio), QIcon::Active);
//        break;
    case SP_MediaSeekForward:
        icon.addPixmap(Skin::pixmap(QLatin1String("forward_grey.png"), QSize(16, 16), Qt::KeepAspectRatio));
        icon.addPixmap(Skin::pixmap(QLatin1String("forward_blue.png"), QSize(16, 16), Qt::KeepAspectRatio), QIcon::Active);
        break;
    case SP_MediaSeekBackward:
        icon.addPixmap(Skin::pixmap(QLatin1String("backward_grey.png"), QSize(16, 16), Qt::KeepAspectRatio));
        icon.addPixmap(Skin::pixmap(QLatin1String("backward_blue.png"), QSize(16, 16), Qt::KeepAspectRatio), QIcon::Active);
        break;
    case SP_MediaSkipForward:
        icon.addPixmap(Skin::pixmap(QLatin1String("rewind_forward_grey.png"), QSize(16, 16), Qt::KeepAspectRatio));
        icon.addPixmap(Skin::pixmap(QLatin1String("rewind_forward_blue.png"), QSize(16, 16), Qt::KeepAspectRatio), QIcon::Active);
        break;
    case SP_MediaSkipBackward:
        icon.addPixmap(Skin::pixmap(QLatin1String("rewind_backward_grey.png"), QSize(16, 16), Qt::KeepAspectRatio));
        icon.addPixmap(Skin::pixmap(QLatin1String("rewind_backward_blue.png"), QSize(16, 16), Qt::KeepAspectRatio), QIcon::Active);
        break;*/
    case SP_MediaVolume:
        icon.addPixmap(Skin::pixmap(QLatin1String("unmute.png"), QSize(16, 16), Qt::KeepAspectRatio));
        break;
    case SP_MediaVolumeMuted:
        icon.addPixmap(Skin::pixmap(QLatin1String("mute.png"), QSize(16, 16), Qt::KeepAspectRatio));
        break;

    default:
        icon = QProxyStyle::standardIconImplementation(standardIcon, option, widget);
        break;
    }

    return icon;
}
