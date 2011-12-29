#include "skin.h"

#include <QtGui/QImage>
#include <QtGui/QPixmapCache>
#include <QtGui/QPainter>
#include <QtGui/QStyleOption>

#include "appstyle_p.h"

#ifndef CL_SKIN_PATH
#  ifdef CL_CUSTOMIZATION_PRESET
#    define CL_SKIN_PATH QLatin1String(":/") + QLatin1String(CL_CUSTOMIZATION_PRESET)
#  else
#    define CL_SKIN_PATH QLatin1Char(':')
#  endif
#endif

QString Skin::path(const QString &name)
{
    return CL_SKIN_PATH + QLatin1String("/skin/") + name;
}

QIcon Skin::icon(const QString &name)
{
    //return QIcon::fromTheme(name, QIcon(path(name)));
    return QIcon(path(name));
}

QPixmap Skin::pixmap(const QString &name, const QSize &size, Qt::AspectRatioMode aspectMode, Qt::TransformationMode mode)
{
    static bool initialized = false;
    if (!initialized) {
        QPixmapCache::setCacheLimit(16 * 1024); // 16 MB
        initialized = true;
    }

    QString key = name;
    if (!size.isEmpty())
        key += QString(QLatin1String("_%1x%2_%3_%4")).arg(int(size.width())).arg(int(size.height())).arg(int(aspectMode)).arg(int(mode));

    QPixmap pixmap;
    if (!QPixmapCache::find(key, &pixmap)) {
        pixmap = QPixmap::fromImage(QImage(Skin::path(name)), Qt::OrderedDither | Qt::OrderedAlphaDither);
        if (!pixmap.isNull()) {
            if (!size.isEmpty() && size != pixmap.size())
                pixmap = pixmap.scaled(size, aspectMode, mode);
        } else {
            qWarning("Skin: Cannot load image `%s`", qPrintable(name));
        }

        QPixmapCache::insert(key, pixmap);
    }

    return pixmap;
}

QStyle *Skin::style()
{
    QString baseStyle;
#ifndef Q_OS_DARWIN
#ifdef CL_CUSTOMIZATION_PRESET
    qputenv("BESPIN_PRESET", QLatin1String(CL_CUSTOMIZATION_PRESET));
#endif
    baseStyle = QLatin1String("Bespin");
#else
    baseStyle = QLatin1String("cleanlooks");
#endif

    return new AppStyle(baseStyle);
}

AppStyle::AppStyle(const QString &baseStyle, QObject *parent)
    : ProxyStyle(baseStyle, parent)
{
}

void AppStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    switch (control) {
#ifndef QT_NO_SLIDER
    case CC_Slider:
        if (const QStyleOptionSlider *sliderOption = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            if (sliderOption->orientation != Qt::Horizontal) {
                qWarning("AppStyle: Non-horizontal sliders are not implemented. Falling back to the default painting.");
                ProxyStyle::drawComplexControl(control, option, painter, widget);
                break;
            }

            const QRect grooveRect = subControlRect(CC_Slider, option, SC_SliderGroove, widget);
            const QRect handleRect = subControlRect(CC_Slider, option, SC_SliderHandle, widget);

            const bool hovered = (option->state & State_Enabled) && (option->activeSubControls & SC_SliderHandle);

            QPixmap grooveBorderPic = Skin::pixmap(QLatin1String("slider_groove_lborder.png"), grooveRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QPixmap grooveBodyPic = Skin::pixmap(QLatin1String("slider_groove_body.png"), grooveRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QPixmap handlePic = Skin::pixmap(hovered ? QLatin1String("slider_handle_active.png") : QLatin1String("slider_handle.png"), 2 * handleRect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

            painter->drawTiledPixmap(grooveRect.adjusted(grooveBorderPic.width(), 0, -grooveBorderPic.width(), 0), grooveBodyPic);
            painter->drawPixmap(grooveRect.topLeft(), grooveBorderPic);
            {
                QTransform oldTransform = painter->transform();
                painter->translate(grooveRect.left() + grooveRect.width(), grooveRect.top());
                painter->scale(-1.0, 1.0);
                painter->drawPixmap(0, 0, grooveBorderPic);
                painter->setTransform(oldTransform);
            }

            painter->drawPixmap(handleRect.topLeft() - QPoint(handleRect.width(), handleRect.height()) / 2, handlePic);
        }
        break;
#endif // QT_NO_SLIDER

    default:
        ProxyStyle::drawComplexControl(control, option, painter, widget);
    }
}

QRect AppStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const
{
    QRect ret;

    switch (control) {
    case CC_TitleBar:
        if (const QStyleOptionTitleBar *tb = qstyleoption_cast<const QStyleOptionTitleBar *>(option)) {
            const int controlMargin = 3;
            const int controlHeight = tb->rect.height() - controlMargin * 2;
            const int delta = controlHeight + controlMargin * 2;
            int offset = 0;

            const bool isMinimized = tb->titleBarState & Qt::WindowMinimized;
            const bool isMaximized = tb->titleBarState & Qt::WindowMaximized;
            const bool isFullScreen = tb->titleBarState & Qt::WindowFullScreen;

            SubControl sc = subControl;
            if (sc == SC_TitleBarNormalButton) { // check what it's good for
               if (isMinimized)
                  sc = SC_TitleBarMinButton;
               else if (isMaximized)
                  sc = SC_TitleBarMaxButton;
               else
                  break;
            }

            switch (sc) {
            case SC_TitleBarLabel:
                if (tb->titleBarFlags & (Qt::WindowTitleHint | Qt::WindowSystemMenuHint)) {
                    ret = tb->rect;
                    if (tb->titleBarFlags & Qt::WindowSystemMenuHint)
                        ret.adjust(delta, 0, -delta, 0);
                    if (tb->titleBarFlags & Qt::WindowShadeButtonHint)
                        ret.adjust(0, 0, -delta, 0);
                    if (tb->titleBarFlags & Qt::WindowMinimizeButtonHint)
                        ret.adjust(0, 0, -delta, 0);
                    if (tb->titleBarFlags & Qt::WindowMaximizeButtonHint)
                        ret.adjust(0, 0, -delta, 0);
                    if (tb->titleBarFlags & Qt::WindowContextHelpButtonHint)
                        ret.adjust(0, 0, -delta, 0);
                    ret.adjust(controlMargin, 0, -controlMargin, 0);
                }
                break;
            case SC_TitleBarContextHelpButton:
                if (tb->titleBarFlags & Qt::WindowContextHelpButtonHint)
                    offset += delta;
                // fall through
            case SC_TitleBarMinButton:
                if (!isMinimized && (tb->titleBarFlags & Qt::WindowMinimizeButtonHint))
                    offset += delta;
                else if (sc == SC_TitleBarMinButton)
                    break;
                // fall through
            case SC_TitleBarMaxButton:
                if (!isMaximized && (tb->titleBarFlags & Qt::WindowMaximizeButtonHint))
                    offset += delta;
                else if (sc == SC_TitleBarMaxButton)
                    break;
                // fall through
            case SC_TitleBarShadeButton:
                if (!isFullScreen && (tb->titleBarFlags & Qt::WindowShadeButtonHint))
                    offset += delta;
                else if (sc == SC_TitleBarShadeButton)
                    break;
                // fall through
            case SC_TitleBarUnshadeButton:
                if (isFullScreen && (tb->titleBarFlags & Qt::WindowShadeButtonHint))
                    offset += delta;
                else if (sc == SC_TitleBarUnshadeButton)
                    break;
                // fall through
            case SC_TitleBarCloseButton:
                if (tb->titleBarFlags & Qt::WindowSystemMenuHint)
                    offset += delta;
                else if (sc == SC_TitleBarCloseButton)
                    break;
                ret.setRect(tb->rect.right() - offset, tb->rect.top() + controlMargin,
                            controlHeight, controlHeight);
                break;
            case SC_TitleBarSysMenu:
                if (tb->titleBarFlags & Qt::WindowSystemMenuHint) {
                    ret.setRect(tb->rect.left() + controlMargin * 2, tb->rect.top() + controlMargin,
                                controlHeight, controlHeight);
                }
                break;

            default:
                break;
            }
            ret = visualRect(tb->direction, tb->rect, ret);
        }
        break;

    default:
        ret = ProxyStyle::subControlRect(control, option, subControl, widget);
        break;
    }

    return ret;
}

int AppStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const
{
    if (hint == QStyle::SH_ToolTipLabel_Opacity)
        return 255;

    return ProxyStyle::styleHint(hint, option, widget, returnData);
}

int AppStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    if (metric == QStyle::PM_TitleBarHeight)
        return 24;

    return ProxyStyle::pixelMetric(metric, option, widget);
}

QPixmap AppStyle::standardPixmap(StandardPixmap standardPixmap, const QStyleOption *option, const QWidget *widget) const
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
    case SP_TitleBarUnshadeButton:
        pm = Skin::pixmap(QLatin1String("decorations/togglefullscreen.png"));
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
        pm = ProxyStyle::standardPixmap(standardPixmap, option, widget);
        break;
    }

    return pm;
}

QIcon AppStyle::standardIconImplementation(StandardPixmap standardIcon, const QStyleOption *option, const QWidget *widget) const
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
    case SP_TitleBarUnshadeButton:
        icon.addPixmap(Skin::pixmap(QLatin1String("decorations/togglefullscreen.png"), QSize(16, 16), Qt::KeepAspectRatio));
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
        icon = ProxyStyle::standardIconImplementation(standardIcon, option, widget);
        break;
    }

    return icon;
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
