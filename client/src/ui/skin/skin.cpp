#include "skin.h"

#include <QtGui/QImage>
#include <QtGui/QPixmapCache>
#include <QtGui/QPainter>
#include <QtGui/QStyleOption>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/warnings.h>
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

int AppStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const
{
    if (hint == QStyle::SH_ToolTipLabel_Opacity)
        return 255;

    return ProxyStyle::styleHint(hint, option, widget, returnData);
}

QIcon AppStyle::standardIconImplementation(StandardPixmap standardIcon, const QStyleOption *option, const QWidget *widget) const
{
    QIcon icon;
    switch (standardIcon) {
    //case SP_TitleBarMenuButton: // ###
    case SP_TitleBarMinButton:
        icon.addPixmap(Skin::pixmap(QLatin1String("decorations/minimize.png"), QSize(16, 16), Qt::KeepAspectRatio));
        break;
    //case SP_TitleBarMaxButton: // ###
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
    //case SP_TitleBarContextHelpButton: // ###
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
    /*case SP_MediaPlay:
        icon.addPixmap(Skin::pixmap(QLatin1String("play_grey.png"), QSize(16, 16), Qt::KeepAspectRatio));
        icon.addPixmap(Skin::pixmap(QLatin1String("play_blue.png"), QSize(16, 16), Qt::KeepAspectRatio), QIcon::Active);
        break;
    case SP_MediaPause:
        icon.addPixmap(Skin::pixmap(QLatin1String("pause_grey.png"), QSize(16, 16), Qt::KeepAspectRatio));
        icon.addPixmap(Skin::pixmap(QLatin1String("pause_blue.png"), QSize(16, 16), Qt::KeepAspectRatio), QIcon::Active);
        break;
//        case SP_MediaStop:
//            icon.addPixmap(Skin::pixmap(QLatin1String("stop_grey.png"), QSize(16, 16), Qt::KeepAspectRatio));
//            icon.addPixmap(Skin::pixmap(QLatin1String("stop_blue.png"), QSize(16, 16), Qt::KeepAspectRatio), QIcon::Active);
//            break;
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

void AppStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    if (control == CC_Slider) {
        if (const QStyleOptionSlider *sliderOption = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            if (sliderOption->orientation != Qt::Horizontal) {
                qWarning("Non-horizontal sliders are not implemented. Falling back to the default painting.");
                ProxyStyle::drawComplexControl(control, option, painter, widget);
                return;
            }
        }

        const QRect grooveRect = subControlRect(CC_Slider, option, SC_SliderGroove, widget);
        const QRect handleRect = subControlRect(CC_Slider, option, SC_SliderHandle, widget);

        //bool hovered = (option->state & State_Enabled) && (option->activeSubControls & SC_SliderHandle);

        QPixmap grooveBorderPic = Skin::pixmap(QLatin1String("slider_groove_lborder.png"), grooveRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPixmap grooveBodyPic = Skin::pixmap(QLatin1String("slider_groove_body.png"), grooveRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPixmap handlePic = Skin::pixmap(QLatin1String("slider_handle.png"), 2 * handleRect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

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

        return;
    }

    ProxyStyle::drawComplexControl(control, option, painter, widget);
}
