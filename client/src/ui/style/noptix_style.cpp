#include "noptix_style.h"
#include <QApplication>
#include <QPainter>
#include <QStyleOption>
#include <QMenu>
#include <QAction>
#include <QSet>
#include <QToolBar>
#include <QAbstractItemView>
#include <utils/common/scoped_painter_rollback.h>
#include "noptix_style_animator.h"
#include "globals.h"
#include "skin.h"

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
    m_animator(new QnNoptixStyleAnimator(this))
{
    GraphicsStyle::setBaseStyle(this);

    m_branchClosed = m_skin->icon("branch_closed.png");
    m_branchOpen = m_skin->icon("branch_open.png");
    m_closeTab = m_skin->icon("decorations/close_tab.png");

    m_grooveBorder = m_skin->pixmap("slider_groove_lborder.png");
    m_grooveBody = m_skin->pixmap("slider_groove_body.png");
    m_sliderHandleHovered = m_skin->pixmap("slider_handle_hovered.png");
    m_sliderHandle = m_skin->pixmap("slider_handle.png");
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
            bool ok;
            int result = target->property(Qn::SliderLength).toInt(&ok);
            if(ok)
                return result;
        }
        break;
    default:
        break;
    }

    return base_type::pixelMetric(metric, option, widget);
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
    default:
        return base_type::styleHint(hint, option, widget, returnData);
    }
}

void QnNoptixStyle::polish(QApplication *application) {
    base_type::polish(application);

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
        
        QFont font = itemView->font();
        font.setPointSize(10);
        itemView->setFont(font);
    } else if(QTabBar *tabBar = qobject_cast<QTabBar *>(widget)) {
        if(tabBar->inherits("QnLayoutTabBar")) {
            QFont font = tabBar->font();
            font.setPointSize(10);
            tabBar->setFont(font);
        }
    }
}

void QnNoptixStyle::unpolish(QWidget *widget) {
    base_type::unpolish(widget);
}


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
    if(!action || !action->property(Qn::HideCheckBoxInMenu).value<bool>()) 
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

    /* If an editor is opened, don't draw item's text. 
     * Editor's background may be transparent, and item text will shine through. */

    QStyleOptionViewItemV4 *localOption = const_cast<QStyleOptionViewItemV4 *>(itemOption);
    QString text = localOption->text;
    localOption->text = QString();
    base_type::drawControl(CE_ItemViewItem, option, painter, widget);
    localOption->text = text;
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
        grooveBorderPic = m_skin->pixmap("slider_groove_lborder.png", grooveRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        grooveBodyPic = m_skin->pixmap("slider_groove_body.png", grooveRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        handlePic = m_skin->pixmap(hovered ? "slider_handle_hovered.png" : "slider_handle.png", handleRect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
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
    buttonOption.state &= ~State_Selected; /* We don't want 'selected' state overriding 'active'. */
    buttonOption.subControls = 0;
    buttonOption.activeSubControls = 0;
    buttonOption.icon = m_closeTab;
    buttonOption.toolButtonStyle = Qt::ToolButtonIconOnly;

    return drawToolButtonComplexControl(&buttonOption, painter, widget);
}

bool QnNoptixStyle::drawBranchPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
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

    QVariant value = widget->property(Qn::ItemViewItemBackgroundOpacity);
    if(!value.isValid())
        return false;

    qreal opacity = painter->opacity();
    painter->setOpacity(opacity * value.toReal());
    base_type::drawPrimitive(element, option, painter, widget);
    painter->setOpacity(opacity);
    return true;
}

bool QnNoptixStyle::drawToolButtonComplexControl(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const {
    const QStyleOptionToolButton *buttonOption = qstyleoption_cast<const QStyleOptionToolButton *>(option);
    if (!buttonOption) 
        return false;

    if (buttonOption->features & QStyleOptionToolButton::Arrow)
        return false; /* We don't handle arrow tool buttons,... */

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



