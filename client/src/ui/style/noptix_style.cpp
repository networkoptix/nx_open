#include "noptix_style.h"

#include <cmath> /* For std::fmod. */

#include <QtCore/QSet>
#include <QtWidgets/QApplication>
#include <QtGui/QPainter>
#include <QtGui/QImage>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QMenu>
#include <QtWidgets/QAction>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QHeaderView>

#include <ui/common/text_pixmap_cache.h>
#include <ui/common/geometry.h>
#include <ui/style/noptix_style_animator.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/widgets/palette_widget.h>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/variant.h>
#include <utils/common/util.h>
#include <utils/math/linear_combination.h>
#include <utils/math/color_transformations.h>
#include <ui/common/text_pixmap_cache.h>
#include <ui/common/geometry.h>
#include <ui/customization/customizer.h>

namespace {
    const char *qn_hoveredPropertyName = "_qn_hovered";
} // anonymous namespace


// -------------------------------------------------------------------------- //
// QnNoptixStyle
// -------------------------------------------------------------------------- //
QnNoptixStyle::QnNoptixStyle(QStyle *style):
    base_type(style),
    m_hoverAnimator(new QnNoptixStyleAnimator(this)),
    m_rotationAnimator(new QnNoptixStyleAnimator(this)),
    m_skin(qnSkin),
    m_customizer(qnCustomizer)
{
    m_branchClosed = m_skin->icon("tree/branch_closed.png");
    m_branchOpen = m_skin->icon("tree/branch_open.png");
}

QnNoptixStyle::~QnNoptixStyle() {
    return;
}

QPixmap QnNoptixStyle::generatedIconPixmap(QIcon::Mode iconMode, const QPixmap &pixmap, const QStyleOption *option) const {
    if(iconMode == QIcon::Disabled) {
        QImage image = QImage(pixmap.size(), QImage::Format_ARGB32);
        image.fill(qRgba(0, 0, 0, 0));

        QPainter painter(&image);
#ifdef Q_OS_LINUX
        painter.setOpacity(0.6);
#else
        painter.setOpacity(0.3);
#endif
        painter.drawPixmap(0, 0, pixmap);
        painter.end();

        return QPixmap::fromImage(image);
    } else {
        return base_type::generatedIconPixmap(iconMode, pixmap, option);
    }
}

void QnNoptixStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const {
    switch (control) {
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
    default:
        return base_type::styleHint(hint, option, widget, returnData);
    }
}

void QnNoptixStyle::polish(QApplication *application) {
    base_type::polish(application);

    QFont font;
    font.setPixelSize(13);
    font.setStyle(QFont::StyleNormal);
    font.setWeight(QFont::Normal);
    font.setFamily(lit("Roboto"));
    application->setFont(font);

    m_customizer->customize(application);
}

void QnNoptixStyle::unpolish(QApplication *application) {
    base_type::unpolish(application);
}

void QnNoptixStyle::polish(QWidget *widget) {
    base_type::polish(widget);

    /* QWidget::scroll method has caching issues leading to some garbage drawn in the updated areas.
        As a workaround we force it to repaint all contents. */
    if(QHeaderView *headerView = dynamic_cast<QHeaderView *>(widget))
        headerView->viewport()->setAutoFillBackground(false);

    if(QAbstractButton *button = dynamic_cast<QAbstractButton *>(widget))
        button->setIcon(m_skin->icon(button->icon()));

    m_customizer->customize(widget);
}

void QnNoptixStyle::unpolish(QWidget *widget) {
    base_type::unpolish(widget);
}

void QnNoptixStyle::polish(QGraphicsWidget *widget) {
    GraphicsStyle::polish(widget);

    m_customizer->customize(widget);
}

void QnNoptixStyle::unpolish(QGraphicsWidget *widget) {
    GraphicsStyle::unpolish(widget);
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

bool QnNoptixStyle::drawBranchPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    Q_UNUSED(widget);

    if(!option->rect.isValid())
        return false;

    if(widget->rect().bottom() < option->rect.bottom() && widget->property(Qn::HideLastRowInTreeIfNotEnoughSpace).toBool())
        return true;

    if (option->state & State_Children) {
        const QIcon &icon = (option->state & State_Open) ? m_branchOpen : m_branchClosed;
        icon.paint(painter, option->rect);
    }

    return true;
}

bool QnNoptixStyle::drawPanelItemViewPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    if(!widget)
        return false;

    if(widget->rect().bottom() < option->rect.bottom() && widget->property(Qn::HideLastRowInTreeIfNotEnoughSpace).toBool())
        return true; /* Draw nothing. */

    qreal itemOpacity = qvariant_cast<qreal>(widget->property(Qn::ItemViewItemBackgroundOpacity), 1.0);
    if(qFuzzyCompare(itemOpacity, 1.0))
        return false; /* Let the default implementation handle it. */

    qreal opacity = painter->opacity();
    painter->setOpacity(opacity * itemOpacity);
    base_type::drawPrimitive(element, option, painter, widget);
    painter->setOpacity(opacity);
    return true;
}

// -------------------------------------------------------------------------- //
// Hover animations
// -------------------------------------------------------------------------- //
void QnNoptixStyle::setHoverProgress(const QWidget *widget, qreal value) const {
    m_hoverAnimator->setValue(widget, value);
}

void QnNoptixStyle::stopHoverTracking(const QWidget *widget) const {
    m_hoverAnimator->stop(widget);
}

qreal QnNoptixStyle::hoverProgress(const QStyleOption *option, const QWidget *widget, qreal speed) const {
    bool hovered = (option->state & State_Enabled) && (option->state & State_MouseOver);

    /* Get animation progress & stop animation if necessary. */
    qreal progress = m_hoverAnimator->value(widget, 0.0);
    if(progress < 0.0 || progress > 1.0) {
        progress = qBound(0.0, progress, 1.0);
        m_hoverAnimator->stop(widget);
    }

    /* Update animation if needed. */
    bool wasHovered = widget->property(qn_hoveredPropertyName).toBool();
    const_cast<QWidget *>(widget)->setProperty(qn_hoveredPropertyName, hovered);
    if(hovered != wasHovered) {
        if(hovered) {
            m_hoverAnimator->start(widget, speed, progress);
        } else {
            m_hoverAnimator->start(widget, -speed, progress);
        }
    }

    return progress;
}
