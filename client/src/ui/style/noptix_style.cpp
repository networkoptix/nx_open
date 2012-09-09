#include "noptix_style.h"

#include <cmath> /* For std::fmod. */

#include <QApplication>
#include <QPainter>
#include <QStyleOption>
#include <QMenu>
#include <QAction>
#include <QSet>
#include <QToolBar>
#include <QAbstractItemView>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/variant.h>
#include <utils/common/util.h>

#include <ui/common/text_pixmap_cache.h>
#include <ui/common/linear_combination.h>
#include <ui/common/color_transformations.h>
#include <ui/common/geometry.h>

#include "noptix_style_animator.h"
#include "globals.h"
#include "skin.h"
#include "ui/widgets/palette_widget.h"

namespace {
    const char *qn_hoveredPropertyName = "_qn_hovered";

    bool hasMenuIndicator(const QStyleOptionToolButton *option) {
        /* Subcontrol requested? */
        bool result = (option->subControls & QStyle::SC_ToolButtonMenu) || (option->features & QStyleOptionToolButton::Menu);

        /* Delayed menu? */
        const QStyleOptionToolButton::ToolButtonFeatures menuFeatures = QStyleOptionToolButton::HasMenu | QStyleOptionToolButton::PopupDelay;
        if (!result)
            result = (option->features & menuFeatures) == menuFeatures;

        return result;
    }

} // anonymous namespace


// -------------------------------------------------------------------------- //
// QnNoptixStyle
// -------------------------------------------------------------------------- //
QnNoptixStyle::QnNoptixStyle(QStyle *style): 
    base_type(style),
    m_skin(qnSkin),
    m_globals(qnGlobals),
    m_animator(new QnNoptixStyleAnimator(this))
{
    GraphicsStyle::setBaseStyle(this);

    m_branchClosed = m_skin->icon("tree/branch_closed.png");
    m_branchOpen = m_skin->icon("tree/branch_open.png");
    m_closeTab = m_skin->icon("titlebar/close_tab.png");

    m_grooveBorder = m_skin->pixmap("slider/slider_groove_lborder.png");
    m_grooveBody = m_skin->pixmap("slider/slider_groove_body.png");
    m_sliderHandleHovered = m_skin->pixmap("slider/slider_handle_hovered.png");
    m_sliderHandle = m_skin->pixmap("slider/slider_handle.png");
}

QnNoptixStyle::~QnNoptixStyle() {
    return;
}

int QnNoptixStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const {
    const QObject *target = currentTarget(widget);

    switch(metric) {
    case PM_ToolBarIconSize:
        return 18;
    case PM_SliderLength:
        if(target) {
            int result = qvariant_cast<int>(target->property(Qn::SliderLength), -1);
            if(result >= 0)
                return result;
        }
        break;
    default:
        break;
    }

    return base_type::pixelMetric(metric, option, widget);
}

QRect QnNoptixStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const {
    QRect result;

    if(control == CC_ScrollBar)
        if(scrollBarSubControlRect(option, subControl, widget, &result))
            return result;

    return base_type::subControlRect(control, option, subControl, widget);
}

void QnNoptixStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const {
    switch (control) {
    case CC_Slider:
        if(drawSliderComplexControl(option, painter, widget))
            return;
        break;
    case CC_TitleBar:
        {
            /* Draw background in order to avoid painting artifacts. */
            QColor bgColor = option->palette.color(widget ? widget->backgroundRole() : QPalette::Window);
            if (bgColor != Qt::transparent) {
                bgColor.setAlpha(255);
                painter->fillRect(option->rect, bgColor);
            }
        }
        break;
    case CC_ToolButton:
        if(drawToolButtonComplexControl(option, painter, widget))
            return;
        break;
    default:
        break;
    }

    base_type::drawComplexControl(control, option, painter, widget);
}

void QnNoptixStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    switch(element) {
    case CE_MenuItem:
        if(drawMenuItemControl(option, painter, widget))
            return;
        break;
    case CE_ItemViewItem:
        if(drawItemViewItemControl(option, painter, widget))
            return;
        break;
    case CE_ProgressBar:
        if(drawProgressBarControl(option, painter, widget))
            return;
        break;
    default:
        break;
    }

    base_type::drawControl(element, option, painter, widget);
}

void QnNoptixStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    switch(element) {
    case PE_IndicatorTabClose:
        if(drawTabClosePrimitive(option, painter, widget))
            return;
        break;
    case PE_IndicatorBranch:
        if(drawBranchPrimitive(option, painter, widget))
            return;
        break;
    case PE_PanelItemViewItem:
    case PE_PanelItemViewRow:
        if(drawPanelItemViewPrimitive(element, option, painter, widget))
            return;
        break;
    default:
        break;
    }

    base_type::drawPrimitive(element, option, painter, widget);
}

int QnNoptixStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const {
    switch(hint) {
    case SH_ToolTipLabel_Opacity:
        return 255;
    case SH_Slider_AbsoluteSetButtons:
        return Qt::LeftButton;
    case SH_Slider_StopMouseOverSlider:
        return true;
    default:
        return base_type::styleHint(hint, option, widget, returnData);
    }
}

void QnNoptixStyle::polish(QApplication *application) {
    base_type::polish(application);

    QColor activeColor = withAlpha(m_globals->selectionColor(), 255);

    QPalette palette = application->palette();
    palette.setColor(QPalette::Active, QPalette::Highlight, activeColor);
    palette.setColor(QPalette::Button, activeColor);
    application->setPalette(palette);

    QFont font;
    font.setPixelSize(12);
    font.setStyle(QFont::StyleNormal);
    font.setWeight(QFont::Normal);
    application->setFont(font);

    QFont menuFont;
    menuFont.setFamily(QLatin1String("Bodoni MT"));
    menuFont.setPixelSize(18);
    application->setFont(menuFont, "QMenu");
}

void QnNoptixStyle::unpolish(QApplication *application) {
    application->setFont(QFont(), "QMenu");

    base_type::unpolish(application);
}

void QnNoptixStyle::polish(QWidget *widget) {
    base_type::polish(widget);

    if(QAbstractItemView *itemView = qobject_cast<QAbstractItemView *>(widget)) {
        itemView->setIconSize(QSize(18, 18));
    }
}

void QnNoptixStyle::unpolish(QWidget *widget) {
    base_type::unpolish(widget);
}

bool QnNoptixStyle::scrollBarSubControlRect(const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget, QRect *result) const {
    const QStyleOptionSlider *sliderOption = qstyleoption_cast<const QStyleOptionSlider *>(option);
    if(!sliderOption)
        return false;

    int sliderWidth = proxy()->pixelMetric(PM_ScrollBarExtent, sliderOption, widget);

    bool needSlider = false;
    switch (subControl) {
    case SC_ScrollBarGroove: 
        *result = option->rect;
        break;
    case SC_ScrollBarSubLine: /* Top / left button. */
    case SC_ScrollBarAddLine: /* Bottom / right button. */
        *result = QRect();
        break;
    default:
        needSlider = true; 
        break;
    }

    if (needSlider) {
        int range = sliderOption->maximum - sliderOption->minimum;
        int grooveLength = (sliderOption->orientation == Qt::Horizontal) ? option->rect.width() : option->rect.height();
        
        int sliderLength = grooveLength;
        if (sliderOption->maximum != sliderOption->minimum) {
            sliderLength = (static_cast<qint64>(grooveLength) * sliderOption->pageStep) / (range + sliderOption->pageStep);
            if (sliderLength > grooveLength) {
                sliderLength = grooveLength;
            } else {
                int minimalLength = proxy()->pixelMetric(PM_ScrollBarSliderMin, sliderOption, widget);;
                if (sliderLength < minimalLength)
                    sliderLength = minimalLength;
            }
        }

        int sliderStart = sliderPositionFromValue(sliderOption->minimum, sliderOption->maximum, sliderOption->sliderPosition, grooveLength - sliderLength, sliderOption->upsideDown);
        switch (subControl) {
        case SC_ScrollBarSubPage: /* Between top / left button and slider. */
            if (sliderOption->orientation == Qt::Horizontal)
                *result = QRect(option->rect.x() + 2, option->rect.y(), sliderStart, sliderWidth);
            else
                *result = QRect(option->rect.x(), option->rect.y() + 2, sliderWidth, sliderStart);
            break;
        case SC_ScrollBarAddPage: /* Between bottom / right button and slider. */
            if (sliderOption->orientation == Qt::Horizontal)
                *result = QRect(option->rect.x() + sliderStart + sliderLength - 2, option->rect.y(), grooveLength - (sliderStart + sliderLength), sliderWidth);
            else
                *result = QRect(option->rect.x(), option->rect.y() + sliderStart + sliderLength - 3, sliderWidth, grooveLength - (sliderStart + sliderLength));
            break;
        case SC_ScrollBarSlider:
            if (sliderOption->orientation == Qt::Horizontal)
                *result = QRect(option->rect.x() + sliderStart, option->rect.y(), sliderLength, sliderWidth);
            else
                *result = QRect(option->rect.x(), option->rect.y() + sliderStart, sliderWidth, sliderLength + 1);
            break;
        default:
            break;
        }
    }

    *result = visualRect(sliderOption->direction, option->rect, *result);
    return true;
}


// -------------------------------------------------------------------------- //
// Painting
// -------------------------------------------------------------------------- //
bool QnNoptixStyle::drawMenuItemControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    const QStyleOptionMenuItem *itemOption = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
    if(!itemOption)
        return false;
    
    const QMenu *menu = qobject_cast<const QMenu *>(widget);
    if(!menu) 
        return false;

    /* There are cases when we want an action to be checkable, but do not want the checkbox displayed in the menu. 
     * So we introduce an internal property for this. */
    QAction *action = menu->actionAt(option->rect.center());
    if(!action || !action->property(Qn::HideCheckBoxInMenu).toBool()) 
        return false;
    
    QStyleOptionMenuItem::CheckType checkType = itemOption->checkType;
    QStyleOptionMenuItem *localOption = const_cast<QStyleOptionMenuItem *>(itemOption);
    localOption->checkType = QStyleOptionMenuItem::NotCheckable;
    base_type::drawControl(CE_MenuItem, option, painter, widget);
    localOption->checkType = checkType;
    return true;
}

bool QnNoptixStyle::drawItemViewItemControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    const QStyleOptionViewItemV4 *itemOption = qstyleoption_cast<const QStyleOptionViewItemV4 *>(option);
    if(!itemOption)
        return false;

    const QAbstractItemView *view = qobject_cast<const QAbstractItemView *>(widget);
    if(view == NULL)
        return false;

    QWidget *editor = view->indexWidget(itemOption->index);
    if(editor == NULL)
        return false;

    /* If an editor is opened, don'h draw item's text. 
     * Editor's background may be transparent, and item text will shine through. */

    QStyleOptionViewItemV4 *localOption = const_cast<QStyleOptionViewItemV4 *>(itemOption);
    QString text = localOption->text;
    localOption->text = QString();
    base_type::drawControl(CE_ItemViewItem, option, painter, widget);
    localOption->text = text;
    return true;
}

bool QnNoptixStyle::drawProgressBarControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const{
    /* Bespin's progress bar painting is way too ugly, so we do it our way. */

    const QStyleOptionProgressBarV2 *pb = qstyleoption_cast<const QStyleOptionProgressBarV2 *>(option);
    if (!pb)
        return false;

    int x,y,w,h;
    pb->rect.getRect(&x, &y, &w, &h);

    const bool vertical = (pb->orientation == Qt::Vertical);

    painter->save();
    if (vertical){
        QTransform transform(painter->transform());
        if (pb->bottomToTop)
            painter->setTransform(transform.translate(0.0, h).rotate(-90));
        else
            painter->setTransform(transform.translate(w, 0.0).rotate(90));
        int t = w;
        w = h;
        h = t;
    }

    const bool busy = pb->maximum == pb->minimum;
    
    const qreal progress = busy ? 1.0 : pb->progress / qreal(pb->maximum - pb->minimum);

    qreal animationProgress = 0.0;
    if (!m_animator->isRunning(widget)) {
        m_animator->start(widget, 0.5, animationProgress);
    } else {
        animationProgress = m_animator->value(widget);
        if (animationProgress >= 1.0){
            animationProgress = std::fmod(animationProgress, 1.0);
            m_animator->setValue(widget, animationProgress);
        }
    }

   
    painter->setPen(Qt::NoPen);

    /* Draw progress indicator. */
    if (progress > 0.0) { 
        const QColor baseColor(4, 154, 116); // TODO: take color from config?
        const QColor glareColor = pb->palette.color(QPalette::WindowText);

        QLinearGradient gradient(x, y, x + w, y);
  
        const qreal radius = 0.1;
        const qreal center = animationProgress * (1.0 + radius * 2) - radius;
        const qreal points[] = {0.0, center - radius, center, center + radius, 1.0};
        for(int i = 0; i < arraysize(points); i++) {
            qreal position = points[i];
            if(position < 0.0 || position > 1.0)
                continue;

            qreal alpha = 0.5 + 0.5 * qMin(qAbs(position - center) / radius, 1.0);
            gradient.setColorAt(position, linearCombine(alpha, baseColor, 1.0 - alpha, glareColor));
        }

        painter->setBrush(gradient);
        if (w * progress > 12) {
            painter->drawRoundedRect(x, y, w * progress, y + h, 6, 6);
        } else {
            painter->setClipRegion(QRegion(x, y, 12, y + h, QRegion::Ellipse));
            painter->drawRoundedRect(x - 12, y, 12 + w * progress, y + h, 6, 6);
            painter->setClipping(false);
        }
    }

    /* Draw groove. */
    QLinearGradient gradient(x, 0, x, y + h);
    gradient.setColorAt(0,      toTransparent(pb->palette.color(QPalette::Button).lighter(), 0.5));
    gradient.setColorAt(0.2,    toTransparent(pb->palette.color(QPalette::Button), 0.5));
    gradient.setColorAt(0.4,    toTransparent(pb->palette.color(QPalette::Button), 0.5));
    gradient.setColorAt(0.5,    toTransparent(pb->palette.color(QPalette::Button).darker(), 0.5));
    gradient.setColorAt(1,      toTransparent(pb->palette.color(QPalette::Button).lighter(), 0.5));
    painter->setBrush(gradient);
    painter->setPen(pb->palette.color(QPalette::Window));
    painter->drawRoundedRect(x, y, w, h, 6, 6);

    /* Draw label. */
    if (pb->textVisible) {
        QRect rect(x, y, w, h);
        QnTextPixmapCache *cache = QnTextPixmapCache::instance();
        QPixmap textPixmap = cache->pixmap(pb->text, painter->font(), pb->palette.color(QPalette::WindowText));
        painter->drawPixmap(QnGeometry::aligned(textPixmap.size(), rect, Qt::AlignCenter), textPixmap);
    }

    painter->restore();
    return true;
}

bool QnNoptixStyle::drawSliderComplexControl(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const {
    /* Bespin's slider painting is way too slow, so we do it our way. */
    const QStyleOptionSlider *sliderOption = qstyleoption_cast<const QStyleOptionSlider *>(option);
    if (!sliderOption) 
        return false;
    
    if (sliderOption->orientation != Qt::Horizontal) 
        return false; /* Non-horizontal sliders are not implemented. */
    
    const QRect grooveRect = subControlRect(CC_Slider, option, SC_SliderGroove, widget);
    QRect handleRect = subControlRect(CC_Slider, option, SC_SliderHandle, widget);

    const bool hovered = (option->state & State_Enabled) && (option->activeSubControls & SC_SliderHandle);

    QPixmap grooveBorderPic;
    QPixmap grooveBodyPic;
    QPixmap handlePic;
    if(!widget) {
        grooveBorderPic = m_grooveBorder;
        grooveBodyPic = m_grooveBody;
        handlePic = hovered ? m_sliderHandleHovered : m_sliderHandle;
    } else {
        grooveBorderPic = m_skin->pixmap("slider/slider_groove_lborder.png", grooveRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        grooveBodyPic = m_skin->pixmap("slider/slider_groove_body.png", grooveRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        handlePic = m_skin->pixmap(hovered ? "slider/slider_handle_hovered.png" : "slider/slider_handle.png", handleRect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    int d = grooveRect.height();

    painter->drawPixmap(grooveRect.adjusted(d, 0, -d, 0), grooveBodyPic, grooveBodyPic.rect());
    painter->drawPixmap(QRectF(grooveRect.topLeft(), QSizeF(d, d)), grooveBorderPic, grooveBorderPic.rect());
    {
        QTransform oldTransform = painter->transform();
        painter->translate(grooveRect.left() + grooveRect.width(), grooveRect.top());
        painter->scale(-1.0, 1.0);
        painter->drawPixmap(QRectF(QPointF(0, 0), QSizeF(d, d)), grooveBorderPic, grooveBorderPic.rect());
        painter->setTransform(oldTransform);
    }

    painter->drawPixmap(handleRect, handlePic);
    return true;
}

bool QnNoptixStyle::drawTabClosePrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    QStyleOptionToolButton buttonOption;
    buttonOption.QStyleOption::operator=(*option);
    buttonOption.state &= ~State_Selected; /* We don'h want 'selected' state overriding 'active'. */
    buttonOption.subControls = 0;
    buttonOption.activeSubControls = 0;
    buttonOption.icon = m_closeTab;
    buttonOption.toolButtonStyle = Qt::ToolButtonIconOnly;

    return drawToolButtonComplexControl(&buttonOption, painter, widget);
}

bool QnNoptixStyle::drawBranchPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    Q_UNUSED(widget);

    if(!option->rect.isValid())
        return false;

    if (option->state & State_Children) {
        const QIcon &icon = (option->state & State_Open) ? m_branchOpen : m_branchClosed;
        icon.paint(painter, option->rect);
    }

    return true;
}

bool QnNoptixStyle::drawPanelItemViewPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    if(!widget)
        return false;

    qreal itemOpacity = qvariant_cast<qreal>(widget->property(Qn::ItemViewItemBackgroundOpacity), 1.0);
    if(qFuzzyCompare(itemOpacity, 1.0))
        return false;

    qreal opacity = painter->opacity();
    painter->setOpacity(opacity * itemOpacity);
    base_type::drawPrimitive(element, option, painter, widget);
    painter->setOpacity(opacity);
    return true;
}

bool QnNoptixStyle::drawToolButtonComplexControl(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const {
    const QStyleOptionToolButton *buttonOption = qstyleoption_cast<const QStyleOptionToolButton *>(option);
    if (!buttonOption) 
        return false;

    if (buttonOption->features & QStyleOptionToolButton::Arrow)
        return false; /* We don'h handle arrow tool buttons,... */

    if (qobject_cast<QToolBar *>(widget->parent()))
        return false; /* ...toolbar buttons,... */

    if(hasMenuIndicator(buttonOption))
        return false; /* ...menu buttons,... */

    if(buttonOption->icon.isNull() || buttonOption->toolButtonStyle == Qt::ToolButtonTextOnly)
        return false; /* ...buttons without icons,... */

    if(!buttonOption->text.isNull() && buttonOption->toolButtonStyle != Qt::ToolButtonIconOnly)
        return false; /* ...and buttons with text. */

    bool sunken = option->state & State_Sunken;
    if(sunken)
        setHoverProgress(widget, 0.0);
    qreal k = hoverProgress(option, widget, 1000.0 / qnGlobals->opacityChangePeriod());
    QRectF rect = option->rect;

#ifdef QN_USE_ZOOMING_BUTTONS
    qreal d = sunken ? 0.1 : (0.1 - k * 0.1);
    rect.adjust(rect.width() * d, rect.height() * d, rect.width() * -d, rect.height() * -d);
#endif

    QIcon::Mode mode;
    if(!(option->state & State_Enabled)) {
        mode = QIcon::Disabled;
    } else if(option->state & State_Selected) {
        mode = QIcon::Selected;
    } else if(option->state & State_Sunken) {
        mode = QnSkin::Pressed;
        k = 1.0;
        stopHoverTracking(widget);
    } else if(option->state & State_MouseOver) {
        mode = QIcon::Active;
    } else {
        mode = QIcon::Normal;
    }
    QIcon::State state = option->state & State_On ? QIcon::On : QIcon::Off;

    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);
    QnScopedPainterSmoothPixmapTransformRollback pixmapRollback(painter, true);
    if(mode == QIcon::Active || (mode == QIcon::Normal && !qFuzzyIsNull(k))) {
        QPixmap pixmap0 = buttonOption->icon.pixmap(rect.toAlignedRect().size(), QIcon::Normal, state);
        QPixmap pixmap1 = buttonOption->icon.pixmap(rect.toAlignedRect().size(), QIcon::Active, state);

        qreal o = painter->opacity();
        painter->drawPixmap(rect, pixmap0, pixmap0.rect());
        painter->setOpacity(o * k);
        painter->drawPixmap(rect, pixmap1, pixmap1.rect());
        painter->setOpacity(o);
    } else {
        QPixmap pixmap = m_skin->pixmap(buttonOption->icon, rect.toAlignedRect().size(), mode, state);
        painter->drawPixmap(rect, pixmap, pixmap.rect());
    }
    return true;
}


// -------------------------------------------------------------------------- //
// Hover animations
// -------------------------------------------------------------------------- //
void QnNoptixStyle::setHoverProgress(const QWidget *widget, qreal value) const {
    m_animator->setValue(widget, value);
}

void QnNoptixStyle::stopHoverTracking(const QWidget *widget) const {
    m_animator->stop(widget);
}

qreal QnNoptixStyle::hoverProgress(const QStyleOption *option, const QWidget *widget, qreal speed) const {
    bool hovered = (option->state & State_Enabled) && (option->state & State_MouseOver);

    /* Get animation progress & stop animation if necessary. */
    qreal progress = m_animator->value(widget, 0.0);
    if(progress < 0.0 || progress > 1.0) {
        progress = qBound(0.0, progress, 1.0);
        m_animator->stop(widget);
    }

    /* Update animation if needed. */
    bool wasHovered = widget->property(qn_hoveredPropertyName).toBool();
    const_cast<QWidget *>(widget)->setProperty(qn_hoveredPropertyName, hovered);
    if(hovered != wasHovered) {
        if(hovered) {
            m_animator->start(widget, speed, progress);
        } else {
            m_animator->start(widget, -speed, progress);
        }
    }

    return progress;
}
