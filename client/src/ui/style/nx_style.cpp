#include "nx_style.h"
#include "nx_style_p.h"
#include "globals.h"
#include "skin.h"

#include <QtCore/QtMath>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QStyleOptionButton>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QDateTimeEdit>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QMenu>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QProxyStyle>
#include <QtWidgets/QInputDialog>
#include <private/qfont_p.h>

#include <ui/common/indentation.h>
#include <ui/common/popup_shadow.h>
#include <ui/common/link_hover_processor.h>
#include <ui/delegates/styled_combo_box_delegate.h>
#include <ui/dialogs/common/generic_tabbed_dialog.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/widgets/common/input_field.h>

#include <utils/common/object_companion.h>
#include <utils/common/property_backup.h>
#include <utils/common/scoped_painter_rollback.h>


#define CUSTOMIZE_POPUP_SHADOWS

using namespace style;

namespace
{
    const char* kDelegateClassBackupId = "delegateClass";
    const char* kViewportMarginsBackupId = "viewportMargins";

    const char* kHoveredChildProperty = "_qn_hoveredChild";

    const QSize kSwitchFocusFrameMargins = QSize(4, 4); // 2 at left, 2 at right, 2 at top, 2 at bottom

    const char* kPopupShadowCompanion = "popupShadow";

    const char* kLinkHoverProcessorCompanion = "linkHoverProcessor";

    const int kCompactTabFocusMargin = 2;

    void drawArrow(Direction direction,
                   QPainter *painter,
                   const QRectF &rect,
                   const QColor &color,
                   const int size = Metrics::kArrowSize,
                   const qreal width = 1.2)
    {
        QPainterPath path;

        QRectF arrowRect = QnGeometry::aligned(QSizeF(size, size), rect);

        RectCoordinates rc(arrowRect);

        switch (direction)
        {
            case Up:
                path.moveTo(rc.x(0.0), rc.y(0.7));
                path.lineTo(rc.x(0.5), rc.y(0.2));
                path.lineTo(rc.x(1.0), rc.y(0.7));
                break;

            case Down:
                path.moveTo(rc.x(0.0), rc.y(0.3));
                path.lineTo(rc.x(0.5), rc.y(0.8));
                path.lineTo(rc.x(1.0), rc.y(0.3));
                break;

            case Left:
                path.moveTo(rc.x(0.7), rc.y(0.0));
                path.lineTo(rc.x(0.2), rc.y(0.5));
                path.lineTo(rc.x(0.7), rc.y(1.0));
                break;

            case Right:
                path.moveTo(rc.x(0.3), rc.y(0.0));
                path.lineTo(rc.x(0.8), rc.y(0.5));
                path.lineTo(rc.x(0.3), rc.y(1.0));
                break;
        }

        QPen pen(color, dpr(width));
        pen.setJoinStyle(Qt::MiterJoin);
        pen.setCapStyle(Qt::FlatCap);

        QnScopedPainterPenRollback penRollback(painter, pen);
        QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);

        painter->drawPath(path);
    }

    void drawMenuCheckMark(QPainter *painter, const QRect &rect, const QColor &color)
    {
        QRect checkRect(QPoint(), QSize(Metrics::kMenuCheckSize, Metrics::kMenuCheckSize));
        checkRect.moveTo(rect.left() + (rect.width() - Metrics::kMenuCheckSize) / 2,
                         rect.top() + (rect.height() - Metrics::kMenuCheckSize) / 2);

        RectCoordinates rc(checkRect);

        QPainterPath path;
        path.moveTo(rc.x(0.0), rc.y(0.6));
        path.lineTo(rc.x(0.2), rc.y(0.8));
        path.lineTo(rc.x(0.65), rc.y(0.35));

        QPen pen(color, dpr(1.5));
        pen.setJoinStyle(Qt::MiterJoin);
        pen.setCapStyle(Qt::FlatCap);

        painter->save();

        painter->setPen(pen);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawPath(path);

        painter->restore();
    }

    TabShape tabShape(const QWidget* widget)
    {
        if (!widget)
            return TabShape::Default;

        return static_cast<TabShape>(widget->property(Properties::kTabShape).toInt());
    }

    bool isCheckableButton(const QStyleOption* option)
    {
        if (qstyleoption_cast<const QStyleOptionButton*>(option))
        {
            if (option->state.testFlag(QStyle::State_On) || option->state.testFlag(QStyle::State_Off))
                return true;

            const QAbstractButton* buttonWidget = qobject_cast<const QAbstractButton*>(option->styleObject);
            if (buttonWidget && buttonWidget->isCheckable())
                return true;
        }

        return false;
    }

    /* Checks whether view item contains a checkbox without any text or decoration: */
    bool isCheckboxOnlyItem(const QStyleOptionViewItem& item)
    {
        return (!item.features.testFlag(QStyleOptionViewItem::HasDisplay) || item.text.isEmpty()) &&
               (!item.features.testFlag(QStyleOptionViewItem::HasDecoration) || item.icon.isNull()) &&
                 item.features.testFlag(QStyleOptionViewItem::HasCheckIndicator);
    }

    /* Checks whether view item contains an icon without text or checkbox: */
    bool isIconOnlyItem(const QStyleOptionViewItem& item)
    {
        return !item.features.testFlag(QStyleOptionViewItem::HasDisplay) &&
               !item.features.testFlag(QStyleOptionViewItem::HasCheckIndicator) &&
                item.features.testFlag(QStyleOptionViewItem::HasDecoration) && !item.icon.isNull();
    }

    /* Checks whether specified color is opaque: */
    bool isColorOpaque(const QColor& color)
    {
        return color.alpha() == 255;
    }

    /* Checks whether a widget is in-place line edit in an item view: */
    bool isItemViewEdit(const QWidget* widget)
    {
        return qobject_cast<const QLineEdit*>(widget) && widget->parentWidget() &&
               qobject_cast<const QAbstractItemView *>(widget->parentWidget()->parentWidget());
    }

    /* Checks whether hovered state is set for entire row and should be removed for an item (e.g. checkbox or switch): */
    bool viewItemHoverAdjusted(const QWidget* widget, const QStyleOption* option, QStyleOptionViewItem& adjusted)
    {
        if (!widget || !option->state.testFlag(QStyle::State_MouseOver))
            return false;

        auto viewItem = qstyleoption_cast<const QStyleOptionViewItem*>(option);
        if (!viewItem)
            return false;

        auto index = widget->property(Properties::kHoveredIndexProperty).value<QModelIndex>();
        if (index == viewItem->index)
            return false;

        adjusted = *viewItem;
        adjusted.state &= ~QStyle::State_MouseOver;
        return true;
    }

    bool isTabHovered(const QStyleOption* option, const QWidget* widget)
    {
        if (!option->state.testFlag(QStyle::State_MouseOver))
            return false;

        /* QTabBar marks a tab as hovered even if a child widget is hovered above that tab.
         * To overcome this problem we process hover events and track hovered children: */
        if (widget && widget->property(kHoveredChildProperty).value<QWidget*>())
            return false;

        return true;
    }

    QnIndentation itemViewItemIndentation(const QStyleOptionViewItem* item)
    {
        static const QnIndentation kDefaultIndentation(Metrics::kStandardPadding, Metrics::kStandardPadding);

        auto view = qobject_cast<const QAbstractItemView*>(item->widget);
        if (!view)
            return kDefaultIndentation;

        if (item->viewItemPosition == QStyleOptionViewItem::Middle)
            return kDefaultIndentation;

        QVariant value = view->property(Properties::kSideIndentation);
        if (!value.canConvert<QnIndentation>())
            return kDefaultIndentation;

        QnIndentation indentation = value.value<QnIndentation>();
        switch (item->viewItemPosition)
        {
            case QStyleOptionViewItem::Beginning:
                indentation.setRight(kDefaultIndentation.right());
                break;

            case QStyleOptionViewItem::End:
                indentation.setLeft(kDefaultIndentation.left());
                break;

            case QStyleOptionViewItem::Invalid:
            {
                const QModelIndex& index = item->index;
                if (!index.isValid())
                    return kDefaultIndentation;

                struct VisibilityChecker : public QAbstractItemView
                {
                    using QAbstractItemView::isIndexHidden;
                };

                auto checker = static_cast<const VisibilityChecker*>(view);

                for (int column = index.column() - 1; column > 0; --column)
                {
                    if (!checker->isIndexHidden(index.sibling(index.row(), column)))
                    {
                        indentation.setLeft(kDefaultIndentation.left());
                        break;
                    }
                }

                int columnCount = view->model()->columnCount(view->rootIndex());
                for (int column = index.column() + 1; column < columnCount; ++column)
                {
                    if (!checker->isIndexHidden(index.sibling(index.row(), column)))
                    {
                        indentation.setRight(kDefaultIndentation.right());
                        break;
                    }
                }

                break;
            }

            case QStyleOptionViewItem::OnlyOne:
            default:
                break;
        }

        return indentation;
    }

    template <class T>
    bool isWidgetOwnedBy(const QWidget* widget)
    {
        if (!widget)
            return false;

        for (QWidget* parent = widget->parentWidget(); parent != nullptr; parent = parent->parentWidget())
        {
            if (qobject_cast<const T*>(parent))
                return true;
        }

        return false;
    }

    enum ScrollBarStyle
    {
        CommonScrollBar,
        DropDownScrollBar,
        TextAreaScrollBar
    };

    ScrollBarStyle scrollBarStyle(const QWidget* widget)
    {
        if (isWidgetOwnedBy<QComboBox>(widget))
            return DropDownScrollBar;

        if (isWidgetOwnedBy<QTextEdit>(widget) || isWidgetOwnedBy<QPlainTextEdit>(widget))
            return TextAreaScrollBar;

        return CommonScrollBar;
    }

    /*
     * Class to gain access to protected property QAbstractScrollArea::viewportMargins:
     */
    class ViewportMarginsAccessHack : public QAbstractScrollArea
    {
    public:
        QMargins viewportMargins() const { return QAbstractScrollArea::viewportMargins(); }
        void setViewportMargins(const QMargins& margins) { QAbstractScrollArea::setViewportMargins(margins); }
    };

} // unnamed namespace


QnNxStylePrivate::QnNxStylePrivate() :
    QCommonStylePrivate(),
    palette(),
    idleAnimator(nullptr),
    stateAnimator(nullptr)
{
    Q_Q(QnNxStyle);

    idleAnimator = new QnNoptixStyleAnimator(q);
    stateAnimator = new QnNoptixStyleAnimator(q);
}

QnNxStyle::QnNxStyle() :
    base_type(*(new QnNxStylePrivate()))
{
}

void QnNxStyle::setGenericPalette(const QnGenericPalette &palette)
{
    Q_D(QnNxStyle);
    d->palette = palette;
}

const QnGenericPalette &QnNxStyle::genericPalette() const
{
    Q_D(const QnNxStyle);
    return d->palette;
}

QnPaletteColor QnNxStyle::findColor(const QColor &color) const
{
    Q_D(const QnNxStyle);
    return d->findColor(color);
}

QnPaletteColor QnNxStyle::mainColor(QnNxStyle::Colors::Palette palette) const
{
    Q_D(const QnNxStyle);
    return d->mainColor(palette);
}

void QnNxStyle::drawSwitch(QPainter *painter, const QStyleOption *option, const QWidget *widget) const
{
    Q_D(const QnNxStyle);

    QStyleOptionViewItem optionAdjusted;
    if (viewItemHoverAdjusted(widget, option, optionAdjusted))
        d->drawSwitch(painter, &optionAdjusted, widget);
    else
        d->drawSwitch(painter, option, widget);
}

void QnNxStyle::drawPrimitive(
        PrimitiveElement element,
        const QStyleOption *option,
        QPainter *painter,
        const QWidget *widget) const
{
    Q_D(const QnNxStyle);

    switch (element)
    {
        case PE_FrameFocusRect:
        {
            if (!option->state.testFlag(State_Enabled))
                return;

            QColor color = widget && widget->property(Properties::kAccentStyleProperty).toBool() ?
                                        option->palette.color(QPalette::HighlightedText) :
                                        option->palette.color(QPalette::Highlight);
            color.setAlphaF(0.5);

            QnScopedPainterPenRollback penRollback(painter, QPen(color, 0, Qt::DotLine));
            QnScopedPainterBrushRollback brushRollback(painter, Qt::NoBrush);
            QnScopedPainterAntialiasingRollback aaRollback(painter, false);

            painter->drawRoundedRect(QnGeometry::eroded(QRectF(option->rect), 0.5), 1.0, 1.0);
            return;
        }

        case PE_PanelButtonCommand:
        {
            const bool pressed = option->state.testFlag(State_Sunken);
            const bool hovered = option->state.testFlag(State_MouseOver);

            QnPaletteColor mainColor = findColor(option->palette.button().color());

            if (option->state.testFlag(State_Enabled))
            {
                if (widget && widget->property(Properties::kAccentStyleProperty).toBool())
                    mainColor = this->mainColor(Colors::kBlue);
            }

            QColor buttonColor = mainColor;
            QColor shadowColor = mainColor.darker(2);
            int shadowShift = 1;

            if (pressed)
            {
                buttonColor = mainColor.darker(1);
                shadowShift = -1;
            }
            else if (hovered)
            {
                buttonColor = mainColor.lighter(1);
                shadowColor = mainColor.darker(1);
            }

            QRect rect = option->rect.adjusted(0, 0, 0, -1);

            QnScopedPainterPenRollback penRollback(painter, Qt::NoPen);
            QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);
            QnScopedPainterBrushRollback brushRollback(painter, shadowColor);

            painter->drawRoundedRect(rect.adjusted(0, shadowShift, 0, shadowShift), 2, 2);
            painter->setBrush(buttonColor);
            painter->drawRoundedRect(rect.adjusted(0, qMax(0, -shadowShift), 0, 0), 2, 2);
            return;
        }

        case PE_PanelButtonTool:
        {
            const QTabBar *tabBar = nullptr;
            TabShape shape = TabShape::Default;

            Qt::ArrowType arrowType = Qt::NoArrow;
            if (widget)
            {
                if (auto button = qobject_cast<const QToolButton*>(widget))
                    arrowType = button->arrowType();

                tabBar = qobject_cast<const QTabBar*>(widget->parent());
                shape = tabShape(tabBar);
            }

            const bool pressed = option->state.testFlag(State_Sunken);
            const bool hovered = option->state.testFlag(State_MouseOver);

            QRect rect = option->rect;

            QnPaletteColor mainColor = findColor(option->palette.window().color());
            if (tabBar)
            {
                switch (shape)
                {
                case TabShape::Default:
                    mainColor = mainColor.lighter(3);
                    rect.adjust(0, 2, 0, -1);
                    break;
                case TabShape::Compact:
                    rect.adjust(0, 2, 0, -2);
                    break;
                case TabShape::Rectangular:
                    mainColor = mainColor.lighter(1);
                    break;
                }
            }

            if (option->state.testFlag(State_Enabled))
            {
                if (widget && widget->property(Properties::kAccentStyleProperty).toBool())
                    mainColor = this->mainColor(Colors::kBlue);
            }

            QColor buttonColor = mainColor;
            if (pressed)
                buttonColor = mainColor.darker(1);
            else if (hovered)
                buttonColor = mainColor.lighter(1);

            painter->fillRect(rect, buttonColor);

            if (tabBar)
            {
                QColor lineColor = (shape == TabShape::Rectangular)
                                   ? option->palette.shadow().color()
                                   : mainColor.lighter(1).color();
                lineColor.setAlpha(QnPaletteColor::kMaxAlpha);

                QnScopedPainterPenRollback penRollback(painter, lineColor);
                painter->drawLine(rect.topLeft(), rect.bottomLeft());

                if (arrowType == Qt::LeftArrow && shape != TabShape::Rectangular)
                {
                    painter->setPen(mainColor.darker(2).color());
                    painter->drawLine(rect.topRight(), rect.bottomRight());
                }
            }

            return;
        }

        case PE_FrameLineEdit:
            /* We draw panel with frame already in PE_PanelLineEdit. */
            return;

        case PE_PanelLineEdit:
        {
            if (widget && (qobject_cast<const QAbstractSpinBox *>(widget->parentWidget()) ||
                           qobject_cast<const QComboBox *>(widget->parentWidget()) ||
                           isItemViewEdit(widget)))
            {
                // Do not draw panel for the internal line edit of certain widgets.
                return;
            }

            QRect rect = widget ? widget->rect() : option->rect;

            QnPaletteColor base = findColor(option->palette.color(QPalette::Shadow));
            QnScopedPainterAntialiasingRollback aaRollback(painter, true);

            QColor frameColor;
            QColor brushColor;

            bool focused = option->state.testFlag(State_HasFocus);
            bool readOnly = false;
            bool valid = true;

            if (option->state.testFlag(State_Enabled))
            {
                if (auto lineEdit = qobject_cast<const QLineEdit*>(widget))
                {
                    readOnly = lineEdit->isReadOnly();

                    if (auto inputField = qobject_cast<const QnInputField*>(lineEdit->parent()))
                        valid = inputField->lastValidationResult() != QValidator::Invalid;
                }
                else if (auto plainTextEdit = qobject_cast<const QPlainTextEdit*>(widget))
                {
                    readOnly = plainTextEdit->isReadOnly();
                }
                else if (auto textEdit = qobject_cast<const QTextEdit*>(widget))
                {
                    readOnly = textEdit->isReadOnly();
                }
            }

            if (readOnly)
            {
                /* Read-only input: */
                frameColor = base;
                brushColor = base;
                frameColor.setAlphaF(0.4);
                brushColor.setAlphaF(0.2);
            }
            else if (focused)
            {
                /* Focused input is always drawn as valid: */
                frameColor = base.darker(3);
                brushColor = base.darker(1);
            }
            else
            {
                /* Valid or not valid not focused input: */
                frameColor = valid ? base.darker(1).color() : qnGlobals->errorTextColor();
                brushColor = base;
            }

            QnScopedPainterPenRollback penRollback(painter, frameColor);
            QnScopedPainterBrushRollback brushRollback(painter, brushColor);

            painter->drawRoundedRect(QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5), 1, 1);
            if (focused)
                painter->drawLine(rect.left() + 1, rect.top() + 1.5, rect.right(), rect.top() + 1.5);

            return;
        }

        /**
         * QTreeView draws in PE_PanelItemViewRow:
         *  - hover markers
         *  - selection marker in the branch area
         *
         * All item views draw in PE_PanelItemViewRow:
         *  - alternate background
         */
        case PE_PanelItemViewRow: /* is deprecated, but cannot be avoided in Qt 5 */
        {
            if (auto item = qstyleoption_cast<const QStyleOptionViewItem *>(option))
            {
                /* In case of tree view: */
                if (const QTreeView* tree = qobject_cast<const QTreeView*>(widget))
                {
                    /* Markers can be semi-transparent, so we draw all layers on top of each other. */

                    /* Obtain hover information: */
                    QBrush hoverBrush = option->palette.midlight();
                    bool hasHover = item->state.testFlag(State_MouseOver) && item->state.testFlag(State_Enabled) &&
                        !tree->property(Properties::kSuppressHoverPropery).toBool();

                    bool hoverOpaque = hasHover && isColorOpaque(hoverBrush.color());

                    /* Obtain selection information: */
                    QBrush selectionBrush = option->palette.highlight();
                    bool hasSelection = item->state.testFlag(State_Selected);
                    bool selectionOpaque = hasSelection && isColorOpaque(selectionBrush.color());

                    /* Draw alternate row background if requested: */
                    if (item->features.testFlag(QStyleOptionViewItem::Alternate) && !hoverOpaque && !selectionOpaque)
                        painter->fillRect(item->rect, option->palette.alternateBase());

                    /* Draw hover marker if needed: */
                    if (hasHover && !selectionOpaque)
                        painter->fillRect(item->rect, hoverBrush);

                    /* Draw selection marker if needed, but only in tree views: */
                    if (hasSelection)
                    {
                        /* Update opaque selection color if also hovered: */
                        if (hasHover && selectionOpaque)
                            selectionBrush.setColor(findColor(selectionBrush.color()).lighter(1));

                        painter->fillRect(item->rect, selectionBrush);
                    }
                }
                else
                /* In case of non-tree view: */
                {
                    /* Draw alternate row background if requested: */
                    if (item->features.testFlag(QStyleOptionViewItem::Alternate))
                        painter->fillRect(item->rect, option->palette.alternateBase());
                }

                return;
            }
            break;
        }

        /**
        * QTreeView draws in PE_PanelItemViewItem:
        *  - selection markers for items (but not branch area)
        *
        * All views draw in PE_PanelItemViewRow:
        *  - hover markers
        *  - selection markers
        */
        case PE_PanelItemViewItem:
        {
            if (auto item = qstyleoption_cast<const QStyleOptionViewItem *>(option))
            {
                /* Obtain selection information: */
                QBrush selectionBrush = option->palette.highlight();
                bool hasSelection = item->state.testFlag(State_Selected);
                bool selectionOpaque = hasSelection && isColorOpaque(selectionBrush.color());

                bool hasHover = item->state.testFlag(State_MouseOver);
                if (widget)
                {
                    if (widget->property(Properties::kSuppressHoverPropery).toBool() ||
                        qobject_cast<const QTreeView*>(widget) && item->state.testFlag(State_Enabled))
                    {
                        /* Itemviews with kSuppressHoverPropery should suppress hover. */
                        /* Enabled items of treeview already have hover painted in PE_PanelItemViewRow. */
                        hasHover = false;
                    }
                    else
                    {
                        /* Obtain Nx hovered row information: */
                        QVariant value = widget->property(Properties::kHoveredRowProperty);
                        if (value.isValid())
                            hasHover = value.toInt() == item->index.row();
                    }
                }

                /* Draw hover marker if needed: */
                if (hasHover && !selectionOpaque)
                    painter->fillRect(item->rect, option->palette.midlight());

                /* Draw selection marker if needed: */
                if (hasSelection)
                {
                    /* Update opaque selection color if also hovered: */
                    if (hasHover && selectionOpaque)
                        selectionBrush.setColor(findColor(selectionBrush.color()).lighter(1));

                    painter->fillRect(item->rect, selectionBrush);
                }

                return;
            }
            break;
        }

        case PE_FrameGroupBox:
        {
            if (auto frame = qstyleoption_cast<const QStyleOptionFrame*>(option))
            {
                const bool panel = frame->features.testFlag(QStyleOptionFrame::Flat);
                QnPaletteColor mainColor = findColor(option->palette.color(QPalette::Mid));

                if (panel)
                {
                    QnScopedPainterAntialiasingRollback aaRollback(painter, false);
                    QnScopedPainterPenRollback penRollback(painter, QPen(mainColor.lighter(2).color(), frame->lineWidth));
                    painter->drawLine(frame->rect.topLeft(), frame->rect.topRight());
                }
                else
                {
                    QnScopedPainterAntialiasingRollback aaRollback(painter, true);
                    QnScopedPainterPenRollback penRollback(painter, QPen(mainColor.color(), frame->lineWidth));
                    painter->drawRoundedRect(QnGeometry::eroded(QRectF(frame->rect), 0.5), Metrics::kGroupBoxCornerRadius, Metrics::kGroupBoxCornerRadius);
                }
            }
            return;
        }

        case PE_FrameTabBarBase:
        case PE_FrameTabWidget:
        {
            QRect rect;
            TabShape shape = TabShape::Default;

            if (const QTabWidget* tabWidget = qobject_cast<const QTabWidget*>(widget))
            {
                shape = tabShape(tabWidget->tabBar());
                rect = tabWidget->tabBar()->geometry();
            }
            else if (const QTabBar* tabBar = qobject_cast<const QTabBar*>(widget))
            {
                shape = tabShape(tabBar);
                rect = tabBar->rect();
            }

            rect.setLeft(option->rect.left());
            rect.setRight(option->rect.right());

            QnPaletteColor mainColor = findColor(option->palette.window().color()).lighter(3);

            QnScopedPainterPenRollback penRollback(painter);
            QnScopedPainterAntialiasingRollback aaRollback(painter, false);

            if (shape == TabShape::Default)
            {
                painter->fillRect(rect, mainColor);

                painter->setPen(mainColor.darker(3));
                painter->drawLine(rect.topLeft(), rect.topRight());

                painter->setPen(option->palette.base().color());
                painter->drawLine(rect.bottomLeft(), rect.bottomRight());
            }
            else
            {
                painter->setPen(mainColor);
                painter->drawLine(rect.bottomLeft(), rect.bottomRight());
            }
#if 0
            /*
            TODO: #vkutin Make entire tab bar displaying focus.
            We need to send whole area updates when a tab loses focus.
            */
            if (option->state.testFlag(State_HasFocus))
            {
                QStyleOptionFocusRect focusOption;
                focusOption.QStyleOption::operator=(*option);
                focusOption.rect = rect.adjusted(1, 1, -1, -1);
                proxy()->drawPrimitive(PE_FrameFocusRect, &focusOption, painter, widget);
            }
#endif
            return;
        }

        case PE_IndicatorViewItemCheck:
        {
            QStyleOptionViewItem adjustedOption;
            if (viewItemHoverAdjusted(widget, option, adjustedOption))
                d->drawCheckBox(painter, &adjustedOption, widget);
            else
                d->drawCheckBox(painter, option, widget);

            return;
        }

        case PE_IndicatorCheckBox:
            d->drawCheckBox(painter, option, widget);
            return;

        case PE_IndicatorRadioButton:
            d->drawRadioButton(painter, option, widget);
            return;

        case PE_PanelMenu:
        {
            QBrush backgroundBrush = option->palette.window();
            QnScopedPainterPenRollback penRollback(painter, backgroundBrush.color());
            QnScopedPainterBrushRollback brushRollback(painter, backgroundBrush);
            QnScopedPainterAntialiasingRollback aaRollback(painter, true);
            QColor shadowRoundingColor = option->palette.shadow().color();
            shadowRoundingColor.setAlphaF(0.5);
            painter->fillRect(QRect(option->rect.bottomRight() - QPoint(2, 2), option->rect.bottomRight()), shadowRoundingColor);
            painter->drawRoundedRect(QnGeometry::eroded(QRectF(option->rect), 0.5), 2.0, 2.0);
            return;
        }

        case PE_IndicatorHeaderArrow:
            d->drawSortIndicator(painter, option, widget);
            return;

        case PE_IndicatorTabClose:
        {
            bool selected = option->state.testFlag(QStyle::State_Selected);

            QColor color = option->palette.color(
                    selected ? QPalette::Text : QPalette::Light);

            if (option->state.testFlag(State_MouseOver))
            {
                QnPaletteColor background = findColor(
                        option->palette.color(selected ? QPalette::Midlight : QPalette::Dark)).lighter(1);
                painter->fillRect(option->rect, background);
            }

            d->drawCross(painter, option->rect, color);
            return;
        }

        case PE_IndicatorArrowUp:
        case PE_IndicatorArrowDown:
        case PE_IndicatorArrowLeft:
        case PE_IndicatorArrowRight:
        {
            QColor color = option->palette.text().color();
            int size = Metrics::kArrowSize;
            qreal width = 1.2;

            if (qobject_cast<const QToolButton*>(widget))
            {
                size = widget->height() / 2 - 2;

                if (const QTabBar* tabBar = qobject_cast<const QTabBar*>(widget->parent()))
                {
                    switch (tabShape(tabBar))
                    {
                        case TabShape::Default:
                        case TabShape::Compact:
                            size = dp(14);
                            color = findColor(option->palette.light().color()).darker(2);
                            width = 1.5;
                            break;
                        case TabShape::Rectangular:
                            width = 1.3;
                            break;
                        default:
                            break;
                    }
                }
            }

            Direction direction = Up;
            switch (element)
            {
                case PE_IndicatorArrowUp:
                    direction = Up;
                    break;
                case PE_IndicatorArrowDown:
                    direction = Down;
                    break;
                case PE_IndicatorArrowLeft:
                    direction = Left;
                    break;
                case PE_IndicatorArrowRight:
                    direction = Right;
                    break;
                default:
                    break;
            }

            drawArrow(direction, painter, option->rect, color, size, width);
            return;
        }

        case PE_FrameMenu:
            return;

        case PE_Frame:
        {
            /* Draw rounded frame for combo box drop-down list: */
            if (isWidgetOwnedBy<QComboBox>(widget))
            {
                QBrush brush = option->palette.midlight();
                QnScopedPainterAntialiasingRollback aaRollback(painter, true);
                QnScopedPainterBrushRollback brushRollback(painter, brush);
                QnScopedPainterPenRollback penRollback(painter, brush.color());
                painter->drawRoundedRect(QnGeometry::eroded(QRectF(widget->rect()), 0.5), 2.0, 2.0);
            }
            return;
        }

        default:
            return;
    }

    base_type::drawPrimitive(element, option, painter, widget);
}

void QnNxStyle::drawComplexControl(
        ComplexControl control,
        const QStyleOptionComplex *option,
        QPainter *painter,
        const QWidget *widget) const
{
    switch (control)
    {
        case CC_ComboBox:
        {
            if (auto comboBox = qstyleoption_cast<const QStyleOptionComboBox *>(option))
            {
                bool listOpened = comboBox->state.testFlag(State_On);
                if (comboBox->editable)
                {
                    proxy()->drawPrimitive(PE_PanelLineEdit, comboBox, painter, widget);

                    QRect buttonRect = subControlRect(control, option, SC_ComboBoxArrow, widget);

                    QnPaletteColor mainColor = findColor(comboBox->palette.color(QPalette::Shadow));
                    QnPaletteColor buttonColor;

                    if (listOpened || comboBox->state.testFlag(State_Sunken))
                        buttonColor = mainColor.lighter(1);
                    else if (comboBox->activeSubControls.testFlag(SC_ComboBoxArrow))
                        buttonColor = mainColor.lighter(2);

                    if (buttonColor.isValid())
                    {
                        QnScopedPainterAntialiasingRollback aaRollback(painter, true);
                        QnScopedPainterBrushRollback brushRollback(painter, buttonColor.color());
                        QnScopedPainterPenRollback penRollback(painter, Qt::NoPen);
                        painter->drawRoundedRect(buttonRect, 2, 2);
                        painter->drawRect(buttonRect.adjusted(0, 0, -buttonRect.width() / 2, 0));
                    }
                }
                else
                {
                    QStyleOptionButton buttonOption;
                    buttonOption.QStyleOption::operator=(*comboBox);
                    buttonOption.rect = comboBox->rect;
                    buttonOption.state = comboBox->state & (State_Enabled | State_MouseOver | State_HasFocus | State_KeyboardFocusChange);
                    if (comboBox->state.testFlag(State_On))
                        buttonOption.state |= State_Sunken;

                    proxy()->drawPrimitive(PE_PanelButtonCommand, &buttonOption, painter, widget);

                    if (comboBox->state.testFlag(State_HasFocus))
                    {
                        QStyleOptionFocusRect focusOption;
                        focusOption.QStyleOption::operator=(buttonOption);
                        focusOption.rect = subElementRect(SE_PushButtonFocusRect, &buttonOption, widget);
                        proxy()->drawPrimitive(PE_FrameFocusRect, &focusOption, painter, widget);
                    }
                }

                if (comboBox->subControls.testFlag(SC_ComboBoxArrow))
                {
                    QRectF rect = subControlRect(CC_ComboBox, comboBox, SC_ComboBoxArrow, widget);
                    drawArrow(listOpened ? Up : Down, painter, rect.translated(0, -1), option->palette.color(QPalette::Text));
                }

                return;
            }
            break;
        }

        case CC_Slider:
        {
            if (auto slider = qstyleoption_cast<const QStyleOptionSlider*>(option))
            {
                QRectF grooveRect = proxy()->subControlRect(CC_Slider, option, SC_SliderGroove, widget);
                QRectF handleRect = proxy()->subControlRect(CC_Slider, option, SC_SliderHandle, widget);

                const bool horizontal = slider->orientation == Qt::Horizontal;
                const bool hovered = slider->state.testFlag(State_MouseOver);

                QnPaletteColor mainDark = findColor(slider->palette.color(QPalette::Window));
                QnPaletteColor mainLight = findColor(slider->palette.color(QPalette::WindowText));

                QnScopedPainterPenRollback penRollback(painter);
                QnScopedPainterBrushRollback brushRollback(painter);

                if (slider->subControls.testFlag(SC_SliderGroove))
                {
                    QRectF grooveDrawRect = grooveRect.adjusted(0.5, 0.5, -0.5, -0.5); /* to include border, antialiased */
                    qreal radius = ((horizontal ? grooveDrawRect.height() : grooveDrawRect.width())) * 0.5;

                    painter->setPen(mainDark.darker(1));
                    painter->setBrush(QBrush(mainDark.lighter(hovered ? 6 : 5)));
                    painter->drawRoundedRect(grooveDrawRect, radius, radius);

                    SliderFeatures features = static_cast<SliderFeatures>(option->styleObject ? option->styleObject->property(Properties::kSliderFeatures).toInt() : 0);
                    if (features.testFlag(SliderFeature::FillingUp))
                    {
                        QRectF fillDrawRect = grooveRect.adjusted(1, 1, -1, -1);

                        int pos = sliderPositionFromValue(
                            slider->minimum,
                            slider->maximum,
                            slider->sliderPosition,
                            horizontal ? fillDrawRect.width() : fillDrawRect.height(),
                            slider->upsideDown);

                        if (horizontal)
                        {
                            if (slider->upsideDown)
                                fillDrawRect.setLeft(pos);
                            else
                                fillDrawRect.setRight(pos);
                        }
                        else
                        {
                            if (slider->upsideDown)
                                fillDrawRect.setTop(pos);
                            else
                                fillDrawRect.setBottom(pos);
                        }

                        radius = ((horizontal ? fillDrawRect.height() : fillDrawRect.width())) * 0.5;

                        painter->setPen(Qt::NoPen);
                        painter->setBrush(QBrush(mainLight));
                        painter->drawRoundedRect(fillDrawRect, radius, radius);
                    }
                }

                if (slider->subControls.testFlag(SC_SliderTickmarks))
                {
                    bool ticksAbove = slider->tickPosition & QSlider::TicksAbove;
                    bool ticksBelow = slider->tickPosition & QSlider::TicksBelow;

                    painter->setPen(slider->palette.color(QPalette::Midlight));
                    int tickSize = proxy()->pixelMetric(PM_SliderTickmarkOffset, option, widget);
                    int available = proxy()->pixelMetric(PM_SliderSpaceAvailable, slider, widget);
                    int interval = slider->tickInterval;

                    if (interval <= 0)
                    {
                        interval = slider->singleStep;

                        if (QStyle::sliderPositionFromValue(slider->minimum, slider->maximum, interval, available) -
                            QStyle::sliderPositionFromValue(slider->minimum, slider->maximum, 0, available) < 3)
                        {
                            interval = slider->pageStep;
                        }
                    }
                    if (interval <= 0)
                        interval = 1;

                    int v = slider->minimum;
                    int len = proxy()->pixelMetric(PM_SliderLength, slider, widget);

                    while (v <= slider->maximum + 1)
                    {
                        if (v == slider->maximum + 1 && interval == 1)
                            break;

                        const int v_ = qMin(v, slider->maximum);
                        int pos = sliderPositionFromValue(slider->minimum, slider->maximum,
                            v_, (horizontal
                                ? slider->rect.width()
                                : slider->rect.height()) - len,
                            slider->upsideDown) + len / 2;
                        const int extra = 2;

                        if (horizontal)
                        {
                            if (ticksAbove)
                                painter->drawLine(pos, slider->rect.top() + extra, pos, slider->rect.top() + tickSize);

                            if (ticksBelow)
                                painter->drawLine(pos, slider->rect.bottom() - extra, pos, slider->rect.bottom() - tickSize);

                        }
                        else
                        {
                            if (ticksAbove)
                                painter->drawLine(slider->rect.left() + extra, pos, slider->rect.left() + tickSize, pos);

                            if (ticksBelow)
                                painter->drawLine(slider->rect.right() - extra, pos, slider->rect.right() - tickSize, pos);
                        }

                        // in the case where maximum is max int
                        int nextInterval = v + interval;
                        if (nextInterval < v)
                            break;

                        v = nextInterval;
                    }
                }

                if (slider->subControls.testFlag(SC_SliderHandle))
                {
                    QColor borderColor = mainLight;
                    QColor fillColor = mainDark;

                    if (option->activeSubControls.testFlag(SC_SliderHandle))
                        borderColor = mainLight.lighter(4);
                    else if (hovered)
                        borderColor = mainLight.lighter(2);

                    if (option->state.testFlag(State_Sunken))
                        fillColor = mainDark.lighter(3);

                    painter->setPen(QPen(borderColor, dp(2)));
                    painter->setBrush(QBrush(fillColor));

                    QnScopedPainterAntialiasingRollback rollback(painter, true);
                    painter->drawEllipse(handleRect.adjusted(1, 1, -1, -1));
                }

                if (option->state.testFlag(State_HasFocus))
                {
                    QStyleOptionFocusRect focusOption;
                    focusOption.QStyleOption::operator=(*option);
                    proxy()->drawPrimitive(QStyle::PE_FrameFocusRect, &focusOption, painter, widget);
                }

                return;
            }
            break;
        }

        case CC_GroupBox:
        {
            if (auto groupBox = qstyleoption_cast<const QStyleOptionGroupBox*>(option))
            {
                const bool panel = groupBox->features.testFlag(QStyleOptionFrame::Flat);
                QRect labelRect = subControlRect(CC_GroupBox, groupBox, SC_GroupBoxLabel, widget);

                if (groupBox->subControls.testFlag(SC_GroupBoxFrame))
                {
                    QStyleOptionFrame frame;
                    frame.QStyleOption::operator=(*groupBox);
                    frame.features = groupBox->features;
                    frame.lineWidth = groupBox->lineWidth;
                    frame.midLineWidth = 0;
                    frame.rect = subControlRect(CC_GroupBox, groupBox, SC_GroupBoxFrame, widget);

                    QnScopedPainterClipRegionRollback clipRollback(painter);
                    if (!panel && labelRect.isValid())
                        painter->setClipRegion(QRegion(option->rect).subtracted(labelRect));

                    drawPrimitive(PE_FrameGroupBox, &frame, painter, widget);
                }

                if (groupBox->subControls.testFlag(SC_GroupBoxLabel))
                {
                    if (panel)
                    {
                        const int kTextFlags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextHideMnemonic;

                        QString text = groupBox->text;
                        QString detailText;

                        int splitPos = text.indexOf(QLatin1Char('\t'));
                        if (splitPos >= 0)
                        {
                            detailText = text.mid(splitPos + 1);
                            text = text.left(splitPos);
                        }

                        QRect rect = labelRect;
                        if (!text.isEmpty())
                        {
                            QFont font = painter->font();
                            font.setPixelSize(font.pixelSize() + 2);
                            font.setWeight(QFont::DemiBold);

                            QnScopedPainterFontRollback fontRollback(painter, font);

                            drawItemText(painter, rect, kTextFlags, groupBox->palette,
                                groupBox->state.testFlag(QStyle::State_Enabled),
                                text, QPalette::Text);

                            rect.setLeft(rect.left() + QFontMetrics(font).width(text, -1, kTextFlags) + Metrics::kStandardPadding);
                        }

                        if (!detailText.isEmpty())
                        {
                            drawItemText(painter, rect, kTextFlags, groupBox->palette,
                                groupBox->state.testFlag(QStyle::State_Enabled),
                                detailText, QPalette::WindowText);
                        }
                    }
                    else
                    {
                        const int kTextFlags = Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextHideMnemonic;
                        drawItemText(painter, labelRect, kTextFlags, groupBox->palette,
                            groupBox->state.testFlag(QStyle::State_Enabled),
                            groupBox->text, QPalette::WindowText);
                    }
                }

                if (groupBox->subControls.testFlag(SC_GroupBoxCheckBox))
                {
                    QStyleOption opt = *option;
                    opt.rect = subControlRect(CC_GroupBox, groupBox, SC_GroupBoxCheckBox, widget);
                    drawSwitch(painter, &opt, widget);
                }

                return;
            }
            break;
        }

        case CC_SpinBox:
        {
            if (auto spinBox = qstyleoption_cast<const QStyleOptionSpinBox*>(option))
            {
                auto drawArrowButton = [&](QStyle::SubControl subControl)
                {
                    QRect buttonRect = subControlRect(control, spinBox, subControl, widget);

                    QnPaletteColor mainColor = findColor(spinBox->palette.color(QPalette::Button));
                    QColor buttonColor;

                    bool up = (subControl == SC_SpinBoxUp);
                    bool enabled = spinBox->state.testFlag(QStyle::State_Enabled) && spinBox->stepEnabled.testFlag(up ? QSpinBox::StepUpEnabled : QSpinBox::StepDownEnabled);

                    if (enabled && spinBox->activeSubControls.testFlag(subControl))
                    {
                        if (spinBox->state.testFlag(State_Sunken))
                            buttonColor = mainColor.darker(1);
                        else
                            buttonColor = mainColor;
                    }

                    if (buttonColor.isValid())
                        painter->fillRect(buttonRect, buttonColor);

                    drawArrow(up ? Up : Down,
                        painter,
                        buttonRect,
                        option->palette.color(enabled ? QPalette::Active : QPalette::Disabled, QPalette::Text));
                };

                if (spinBox->subControls.testFlag(SC_SpinBoxFrame) && spinBox->frame)
                    drawPrimitive(PE_PanelLineEdit, spinBox, painter, widget);

                if (spinBox->subControls.testFlag(SC_SpinBoxUp))
                    drawArrowButton(SC_SpinBoxUp);

                if (spinBox->subControls.testFlag(SC_SpinBoxDown))
                    drawArrowButton(SC_SpinBoxDown);

                return;
            }
            break;
        }

        case CC_ScrollBar:
        {
            if (auto scrollBar = qstyleoption_cast<const QStyleOptionSlider*>(option))
            {
                ScrollBarStyle style = scrollBarStyle(widget);

                /* Paint groove only for common scroll bars: */
                if (scrollBar->subControls.testFlag(SC_ScrollBarGroove) && style == CommonScrollBar)
                {
                    QRect grooveRect = proxy()->subControlRect(control, scrollBar, SC_ScrollBarGroove, widget);
                    painter->fillRect(grooveRect, scrollBar->palette.color(QPalette::Dark));
                }

                /* Paint slider: */
                if (scrollBar->subControls.testFlag(SC_ScrollBarSlider))
                {
                    QRect sliderRect;
                    QnPaletteColor sliderColor;

                    const int kSliderMargin = 1;
                    const int kContainerFrame = 1;

                    switch (style)
                    {
                        /* Scroll bar in multiline text edits: */
                        case TextAreaScrollBar:
                        {
                            QStyleOptionSlider optionCopy(*scrollBar);
                            if (scrollBar->orientation == Qt::Vertical)
                            {
                                optionCopy.rect.adjust(0, kContainerFrame, 0, -kContainerFrame);
                                sliderRect = proxy()->subControlRect(control, &optionCopy, SC_ScrollBarSlider, widget);
                                sliderRect.adjust(kSliderMargin, 0, -kSliderMargin, 0);
                            }
                            else
                            {
                                optionCopy.rect.adjust(kContainerFrame, 0, -kContainerFrame, 0);
                                sliderRect = proxy()->subControlRect(control, &optionCopy, SC_ScrollBarSlider, widget);
                                sliderRect.adjust(0, kSliderMargin, 0, -kSliderMargin);
                            }

                            sliderColor = findColor(scrollBar->palette.color(QPalette::Midlight)).darker(1);
                            break;
                        }

                        /* Scroll bar in drop-down list of a combobox: */
                        case DropDownScrollBar:
                        {
                            sliderRect = proxy()->subControlRect(control, scrollBar, SC_ScrollBarSlider, widget);
                            if (scrollBar->orientation == Qt::Vertical)
                                sliderRect.adjust(kSliderMargin, 0, -kSliderMargin, 0);
                            else
                                sliderRect.adjust(0, kSliderMargin, 0, -kSliderMargin);

                            sliderColor = findColor(scrollBar->palette.color(QPalette::WindowText));
                            break;
                        }

                        /* Common scroll bar: */
                        case CommonScrollBar:
                        default:
                            sliderRect = proxy()->subControlRect(control, scrollBar, SC_ScrollBarSlider, widget);
                            sliderColor = findColor(scrollBar->palette.color(QPalette::Midlight));
                            break;
                    }

                    /* Handle hovered & pressed states: */
                    if (scrollBar->state.testFlag(State_Sunken))
                        sliderColor = sliderColor.lighter(1);
                    else if (scrollBar->state.testFlag(State_MouseOver))
                        sliderColor = sliderColor.lighter(scrollBar->activeSubControls.testFlag(SC_ScrollBarSlider) ? 2 : 1);

                    /* Paint: */
                    if (style == CommonScrollBar)
                    {
                        QnScopedPainterAntialiasingRollback aaRollback(painter, false);
                        painter->fillRect(sliderRect, sliderColor);
                    }
                    else
                    {
                        QnScopedPainterAntialiasingRollback aaRollback(painter, true);
                        QnScopedPainterBrushRollback brushRollback(painter, sliderColor.color());
                        QnScopedPainterPenRollback penRollback(painter, sliderColor.color());
                        painter->drawRoundedRect(QRectF(sliderRect).adjusted(0.5, 0.5, -0.5, -0.5), 2.0, 2.0);
                    }
                }

                return;
            }
            break;
        }

        case CC_ToolButton:
        {
            if (auto button = qstyleoption_cast<const QStyleOptionToolButton*>(option))
            {
                QStyleOptionToolButton opt(*button);
                opt.features &= ~QStyleOptionToolButton::HasMenu;
                base_type::drawComplexControl(control, &opt, painter, widget);
                return;
            }
            break;
        }

        default:
            break;
    }

    base_type::drawComplexControl(control, option, painter, widget);
}

void QnNxStyle::drawControl(
        ControlElement element,
        const QStyleOption *option,
        QPainter *painter,
        const QWidget *widget) const
{
    Q_D(const QnNxStyle);

    switch (element)
    {
        case CE_ShapedFrame:
        {
            if (auto frame = qstyleoption_cast<const QStyleOptionFrame*>(option))
            {
                /* Special frame for multiline text edits: */
                if (qobject_cast<const QPlainTextEdit*>(widget) || qobject_cast<const QTextEdit*>(widget))
                {
                    /* Frame entire widget rect, with scrollbars: */
                    QStyleOptionFrame frameCopy(*frame);
                    frameCopy.rect = widget->rect();
                    proxy()->drawPrimitive(PE_PanelLineEdit, &frameCopy, painter, widget);
                    return;
                }

                switch (frame->frameShape)
                {
                    case QFrame::Box:
                    {
                        QnPaletteColor mainColor = findColor(option->palette.color(QPalette::Shadow)).darker(1);
                        QnScopedPainterPenRollback penRollback(painter, mainColor.color());
                        painter->drawRect(frame->rect.adjusted(0, 0, -1, -1));
                        return;
                    }

                    case QFrame::HLine:
                    case QFrame::VLine:
                    {
                        QnPaletteColor mainColor = findColor(option->palette.color(QPalette::Shadow));

                        QColor firstColor = mainColor;
                        QColor secondColor;

                        if (frame->state.testFlag(State_Sunken))
                        {
                            secondColor = mainColor.lighter(4);
                        }
                        else if (frame->state.testFlag(State_Raised))
                        {
                            secondColor = firstColor;
                            firstColor = mainColor.lighter(4);
                        }

                        QRect rect = frame->rect;

                        if (frame->frameShape == QFrame::HLine)
                        {
                            rect.setHeight(frame->lineWidth);
                            painter->fillRect(rect, firstColor);

                            if (secondColor.isValid())
                            {
                                rect.moveTop(rect.top() + rect.height());
                                painter->fillRect(rect, secondColor);
                            }
                        }
                        else
                        {
                            rect.setLeft(rect.left() + (rect.width() - frame->lineWidth + 1) / 2); /* - center-align vertical lines */
                            rect.setWidth(frame->lineWidth);
                            painter->fillRect(rect, firstColor);

                            if (secondColor.isValid())
                            {
                                rect.moveLeft(rect.left() + rect.width());
                                painter->fillRect(rect, secondColor);
                            }
                        }

                        return;
                    }

                    default:
                        break;
                }
            }
            break;
        }

        case CE_ComboBoxLabel:
        {
            if (auto comboBox = qstyleoption_cast<const QStyleOptionComboBox*>(option))
            {
                QStyleOptionComboBox opt = *comboBox;
                opt.rect.setLeft(opt.rect.left() + dp(6));
                base_type::drawControl(element, &opt, painter, widget);
                return;
            }
            break;
        }

        case CE_CheckBoxLabel:
        case CE_RadioButtonLabel:
        {
            if (auto button = qstyleoption_cast<const QStyleOptionButton*>(option))
            {
                QStyleOptionButton opt = *button;
                opt.palette.setColor(QPalette::WindowText, d->checkBoxColor(option, element == CE_RadioButtonLabel));
                base_type::drawControl(element, &opt, painter, widget);
                return;
            }
            break;
        }

        case CE_TabBarTabShape:
        {
            if (auto tab = qstyleoption_cast<const QStyleOptionTab*>(option))
            {
                TabShape shape = tabShape(widget);

                switch (shape)
                {
                    case TabShape::Compact:
                        break;

                    case TabShape::Default:
                    {
                        if (!tab->state.testFlag(State_Selected) && isTabHovered(tab, widget))
                        {
                            QnPaletteColor mainColor = findColor(option->palette.window().color()).lighter(4);
                            painter->fillRect(tab->rect.adjusted(0, 1, 0, -1), mainColor);

                            QnScopedPainterPenRollback penRollback(painter, mainColor.darker(3).color());
                            painter->drawLine(tab->rect.topLeft(), tab->rect.topRight());
                        }
                        break;
                    }

                    case TabShape::Rectangular:
                    {
                        QnPaletteColor mainColor = findColor(
                                option->palette.window().color()).lighter(4);

                        QColor color = mainColor;

                        if (tab->state.testFlag(State_Selected))
                        {
                            color = option->palette.midlight().color();
                        }
                        else if (isTabHovered(tab, widget))
                        {
                            color = mainColor.lighter(1);
                        }

                        painter->fillRect(tab->rect, color);

                        if (tab->position == QStyleOptionTab::Middle ||
                            tab->position == QStyleOptionTab::End)
                        {
                            QnScopedPainterPenRollback penRollback(
                                        painter, option->palette.shadow().color());
                            painter->drawLine(tab->rect.topLeft(),
                                              tab->rect.bottomLeft());
                        }

                        break;
                    }
                }
                return;
            }
            break;
        }

        case CE_TabBarTabLabel:
        {
            if (auto tab = qstyleoption_cast<const QStyleOptionTab*>(option))
            {
                TabShape shape = tabShape(widget);
                int textFlags = Qt::TextHideMnemonic;

                QColor color;

                QRect textRect = tab->rect;
                QRect focusRect = tab->rect;

                if (shape == TabShape::Rectangular)
                {
                    textFlags |= Qt::AlignLeft | Qt::AlignVCenter;

                    if (tab->state.testFlag(QStyle::State_Selected))
                        color = tab->palette.text().color();
                    else
                        color = tab->palette.light().color();

                    int hspace = pixelMetric(PM_TabBarTabHSpace, option, widget);
                    textRect.adjust(hspace, 0, -hspace, 0);
                    focusRect.adjust(0, 2, 0, -2);
                }
                else
                {
                    textFlags |= Qt::AlignCenter;

                    QnPaletteColor mainColor = findColor(tab->palette.light().color()).darker(2);
                    color = mainColor;

                    QFontMetrics fm(painter->font());
                    QRect rect = fm.boundingRect(textRect, textFlags, tab->text);

                    if (shape == TabShape::Compact)
                    {
                        rect.setBottom(textRect.bottom());
                        rect.setTop(rect.bottom());
                        focusRect.setLeft(rect.left() - kCompactTabFocusMargin);
                        focusRect.setRight(rect.right() - kCompactTabFocusMargin);
                    }
                    else
                    {
                        focusRect.adjust(0, 2, 0, -2);
                        rect.setBottom(textRect.bottom() - 1);
                        rect.setTop(rect.bottom() - 1);
                    }

                    if (tab->state.testFlag(QStyle::State_Selected))
                    {
                        color = tab->palette.highlight().color();
                        painter->fillRect(rect, color);
                    }
                    else if (isTabHovered(tab, widget))
                    {
                        if (shape == TabShape::Compact)
                        {
                            color = mainColor.lighter(2);
                            painter->fillRect(rect, findColor(
                                option->palette.window().color()).lighter(6));
                        }
                        else
                        {
                            color = mainColor.lighter(1);
                        }
                    }
                }

                QnScopedPainterPenRollback penRollback(painter, color);

                drawItemText(painter,
                    textRect,
                    textFlags,
                    tab->palette,
                    tab->state.testFlag(QStyle::State_Enabled),
                    tab->text);

                if (tab->state.testFlag(State_HasFocus))
                {
                    QStyleOptionFocusRect focusOption;
                    focusOption.QStyleOption::operator=(*tab);
                    focusOption.rect = focusRect;
                    proxy()->drawPrimitive(PE_FrameFocusRect, &focusOption, painter, widget);
                }
            }
            return;
        }

        case CE_HeaderSection:
        {
            if (auto header = qstyleoption_cast<const QStyleOptionHeader*>(option))
            {
                if (header->state.testFlag(State_MouseOver))
                {
                    QColor color = findColor(header->palette.midlight().color());
                    painter->fillRect(header->rect, color);
                }

                if (header->orientation == Qt::Horizontal)
                {
                    QnScopedPainterPenRollback penRollback(painter, header->palette.color(QPalette::Dark));
                    painter->drawLine(header->rect.bottomLeft(), header->rect.bottomRight());
                }
                return;
            }
            break;
        }

        case CE_HeaderEmptyArea:
        {
            if (auto header = qstyleoption_cast<const QStyleOptionHeader*>(option))
            {
                painter->fillRect(header->rect, header->palette.window());
                return;
            }
            break;
        }

        case CE_MenuItem:
        {
            if (auto menuItem = qstyleoption_cast<const QStyleOptionMenuItem*>(option))
            {
                if (menuItem->menuItemType == QStyleOptionMenuItem::Separator)
                {
                    QnScopedPainterPenRollback penRollback(painter, menuItem->palette.color(QPalette::Midlight));
                    int y = menuItem->rect.top() + menuItem->rect.height() / 2;
                    painter->drawLine(Metrics::kMenuItemHPadding, y,
                                      menuItem->rect.right() - Metrics::kMenuItemHPadding, y);
                    break;
                }

                bool enabled = menuItem->state.testFlag(State_Enabled);
                bool selected = enabled && menuItem->state.testFlag(State_Selected);

                QColor textColor = menuItem->palette.color(QPalette::WindowText);
                QColor backgroundColor = menuItem->palette.color(QPalette::Window);
                QColor shortcutColor = menuItem->palette.color(QPalette::ButtonText);

                if (selected)
                {
                    textColor = menuItem->palette.color(QPalette::HighlightedText);
                    backgroundColor = menuItem->palette.color(QPalette::Highlight);
                    shortcutColor = textColor;
                }

                if (selected)
                    painter->fillRect(menuItem->rect, backgroundColor);

                int xPos = Metrics::kMenuItemTextLeftPadding;
                int y = menuItem->rect.y();

                int textFlags = Qt::AlignVCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
                if (!styleHint(SH_UnderlineShortcut, menuItem, widget, nullptr))
                    textFlags |= Qt::TextHideMnemonic;

                QRect textRect(xPos,
                               y + Metrics::kMenuItemVPadding,
                               menuItem->rect.width() - xPos - Metrics::kMenuItemHPadding,
                               menuItem->rect.height() - 2 * Metrics::kMenuItemVPadding);

                if (!menuItem->text.isEmpty())
                {
                    QnScopedPainterPenRollback penRollback(painter);

                    QString text = menuItem->text;
                    QString shortcut;

                    int tabPosition = text.indexOf(QLatin1Char('\t'));
                    if (tabPosition >= 0)
                    {
                        shortcut = text.mid(tabPosition + 1);
                        text = text.left(tabPosition);

                        painter->setPen(shortcutColor);
                        painter->drawText(textRect, textFlags | Qt::AlignRight, shortcut);
                    }

                    painter->setPen(textColor);
                    painter->drawText(textRect, textFlags | Qt::AlignLeft, text);
                }

                if (menuItem->checked && menuItem->checkType != QStyleOptionMenuItem::NotCheckable)
                {
                    drawMenuCheckMark(
                            painter,
                            QRect(Metrics::kMenuItemHPadding, menuItem->rect.y(), dp(16), menuItem->rect.height()),
                            textColor);
                }

                if (menuItem->menuItemType == QStyleOptionMenuItem::SubMenu)
                {
                    drawArrow(Right,
                              painter,
                              QRect(menuItem->rect.right() - Metrics::kMenuItemVPadding - Metrics::kArrowSize, menuItem->rect.top(),
                                    Metrics::kArrowSize, menuItem->rect.height()),
                              textColor);
                }

                return;
            }
            break;
        }

        case CE_ProgressBarGroove:
        {
            if (auto progressBar = qstyleoption_cast<const QStyleOptionProgressBar *>(option))
            {
                QnPaletteColor mainColor = findColor(progressBar->palette.color(QPalette::Shadow));

                painter->fillRect(progressBar->rect, mainColor);

                bool determined = progressBar->minimum < progressBar->maximum;
                if (determined)
                {
                    QnScopedPainterPenRollback penRollback(painter, QPen(mainColor.lighter(4)));
                    painter->drawLine(progressBar->rect.left(),
                                      progressBar->rect.bottom() + 1,
                                      progressBar->rect.right(),
                                      progressBar->rect.bottom() + 1);
                }

                return;
            }
            break;
        }

        case CE_ProgressBarContents:
        {
            if (auto progressBar = qstyleoption_cast<const QStyleOptionProgressBar *>(option))
            {
                Q_D(const QnNxStyle);

                QRect rect = progressBar->rect;
                QnPaletteColor mainColor = findColor(progressBar->palette.color(QPalette::Highlight));
                bool determined = progressBar->minimum < progressBar->maximum;

                if (determined)
                {
                    d->idleAnimator->stop(widget);

                    int pos = sliderPositionFromValue(
                                progressBar->minimum,
                                progressBar->maximum,
                                progressBar->progress,
                                progressBar->orientation == Qt::Horizontal ? rect.width() : rect.height(),
                                progressBar->invertedAppearance);

                    if (progressBar->orientation == Qt::Horizontal)
                    {
                        if (progressBar->invertedAppearance)
                            rect.setLeft(pos);
                        else
                            rect.setRight(pos);
                    }
                    else
                    {
                        if (progressBar->invertedAppearance)
                            rect.setTop(pos);
                        else
                            rect.setBottom(pos);
                    }

                    painter->fillRect(rect, mainColor);
                }
                else
                {
                    painter->fillRect(rect, mainColor.darker(4));

                    const qreal kTickWidth = M_PI / 2;
                    const qreal kTickSpace = kTickWidth / 2;
                    const qreal kMaxAngle = M_PI + kTickWidth + kTickSpace;
                    const qreal kSpeed = kMaxAngle * 0.3;

                    const QColor color = mainColor.darker(1);

                    qreal angle = d->idleAnimator->value(widget);
                    if (angle >= kMaxAngle)
                    {
                        angle = std::fmod(angle, kMaxAngle);
                        d->idleAnimator->setValue(widget, angle);
                    }

                    if (!d->idleAnimator->isRunning(widget))
                        d->idleAnimator->start(widget, kSpeed, angle);

                    if (angle < M_PI + kTickWidth)
                    {
                        auto f = [](qreal cos) -> qreal
                        {
                            const qreal kFactor = 1.0 / 1.4;
                            return std::copysign(std::pow(std::abs(cos), kFactor), cos);
                        };

                        qreal x1 = (1.0 - f(std::cos(qMax(0.0, angle - kTickWidth)))) / 2.0;
                        qreal x2 = (1.0 - f(std::cos(qMin(M_PI, angle)))) / 2.0;

                        int rx1 = int(rect.left() + rect.width() * x1);
                        int rx2 = int(rect.left() + rect.width() * x2);

                        painter->fillRect(qMax(rx1, rect.left()), rect.top(), rx2 - rx1, rect.height(), color);
                    }
                }

                return;
            }
            break;
        }

        case CE_ProgressBarLabel:
        {
            if (auto progressBar = qstyleoption_cast<const QStyleOptionProgressBar *>(option))
            {
                if (!progressBar->textVisible || progressBar->text.isEmpty())
                    return;

                QString title;
                QString text = progressBar->text;

                int sep = text.indexOf(QLatin1Char('\t'));
                if (sep >= 0)
                {
                    title = text.left(sep);
                    text = text.mid(sep + 1);
                }

                QRect rect = progressBar->rect;

                QFont font = painter->font();
                font.setWeight(QFont::DemiBold);
                font.setPixelSize(font.pixelSize() - 2);

                QnScopedPainterFontRollback fontRollback(painter, font);
                QnScopedPainterPenRollback penRollback(painter);

                if (!title.isEmpty())
                {
                    painter->setPen(progressBar->palette.color(QPalette::Highlight));

                    Qt::Alignment alignment = Qt::AlignBottom |
                                              (progressBar->direction == Qt::LeftToRight ? Qt::AlignLeft : Qt::AlignRight);

                    drawItemText(painter,
                                 rect,
                                 alignment,
                                 progressBar->palette,
                                 progressBar->state.testFlag(QStyle::State_Enabled),
                                 title);
                }

                if (!text.isEmpty())
                {
                    bool determined = progressBar->minimum < progressBar->maximum;

                    Qt::Alignment alignment = Qt::AlignBottom |
                                              (progressBar->direction == Qt::LeftToRight ? Qt::AlignRight : Qt::AlignLeft);

                    painter->setPen(progressBar->palette.color(determined ? QPalette::Text : QPalette::WindowText));

                    drawItemText(painter,
                                 rect,
                                 alignment,
                                 progressBar->palette,
                                 progressBar->state.testFlag(QStyle::State_Enabled),
                                 text);
                }

                return;
            }
            break;
        }

        case CE_PushButtonLabel:
        {
            if (isCheckableButton(option))
            {
                /* Calculate minimal label width: */
                auto buttonOption = static_cast<const QStyleOptionButton*>(option); /* isCheckableButton()==true guarantees type safety */
                int minLabelWidth = 2 * pixelMetric(PM_ButtonMargin, option, widget);
                if (!buttonOption->icon.isNull())
                    minLabelWidth += buttonOption->iconSize.width() + 4; /* 4 is hard-coded in Qt */
                if (!buttonOption->text.isEmpty())
                    minLabelWidth += buttonOption->fontMetrics.size(Qt::TextShowMnemonic, buttonOption->text).width();

                /* Draw standard button content left-aligned: */
                QStyleOptionButton newOpt(*buttonOption);
                newOpt.rect.setWidth(minLabelWidth);
                base_type::drawControl(element, &newOpt, painter, widget);

                /* Draw switch right-aligned: */
                newOpt.rect.setWidth(Metrics::kButtonSwitchSize.width());
                newOpt.rect.moveRight(option->rect.right() - Metrics::kSwitchMargin);
                newOpt.rect.setBottom(newOpt.rect.bottom() - 1); // shadow compensation
                drawSwitch(painter, &newOpt, widget);
                return;
            }

            break;
        }

        case CE_Splitter:
        {
            painter->fillRect(option->rect, option->palette.shadow());
            break;
        }

        default:
            break;
    }

    base_type::drawControl(element, option, painter, widget);
}

QRect QnNxStyle::subControlRect(
        ComplexControl control,
        const QStyleOptionComplex *option,
        SubControl subControl,
        const QWidget *widget) const
{
    QRect rect = base_type::subControlRect(control, option, subControl, widget);

    switch (control)
    {
        case CC_Slider:
        {
            if (auto slider = qstyleoption_cast<const QStyleOptionSlider*>(option))
            {
                int tickSize = proxy()->pixelMetric(PM_SliderTickmarkOffset, option, widget);

                switch (subControl)
                {
                    case SC_SliderHandle:
                    {
                        if (slider->orientation == Qt::Horizontal)
                        {
                            int cy = slider->rect.top() + slider->rect.height() / 2;

                            if (slider->tickPosition & QSlider::TicksAbove)
                                cy += tickSize;
                            if (slider->tickPosition & QSlider::TicksBelow)
                                cy -= tickSize;

                            rect.moveTop(cy - rect.height() / 2);
                        }
                        else
                        {
                            int cx = slider->rect.left() + slider->rect.width() / 2;

                            if (slider->tickPosition & QSlider::TicksAbove)
                                cx += tickSize;
                            if (slider->tickPosition & QSlider::TicksBelow)
                                cx -= tickSize;

                            rect.moveLeft(cx - rect.width() / 2);
                        }
                        break;
                    }

                    case SC_SliderGroove:
                    {
                        const int kGrooveWidth = dp(4);

                        QPoint grooveCenter = slider->rect.center();

                        if (slider->orientation == Qt::Horizontal)
                        {
                            rect.setHeight(kGrooveWidth);
                            if (slider->tickPosition & QSlider::TicksAbove)
                                grooveCenter.ry() += tickSize;
                            if (slider->tickPosition & QSlider::TicksBelow)
                                grooveCenter.ry() -= tickSize;
                        }
                        else
                        {
                            rect.setWidth(kGrooveWidth);
                            if (slider->tickPosition & QSlider::TicksAbove)
                                grooveCenter.rx() += tickSize;
                            if (slider->tickPosition & QSlider::TicksBelow)
                                grooveCenter.rx() -= tickSize;
                        }

                        rect.moveCenter(grooveCenter);
                        break;
                    }

                    default:
                        break;
                }
            }
            break;
        }

        case CC_ComboBox:
        {
            if (auto comboBox = qstyleoption_cast<const QStyleOptionComboBox*>(option))
            {
                switch (subControl)
                {
                    case SC_ComboBoxArrow:
                    {
                        rect = QRect(comboBox->rect.right() - comboBox->rect.height(), 0,
                                     comboBox->rect.height(), comboBox->rect.height());
                        rect.adjust(1, 1, 0, -1);
                        break;
                    }

                    case SC_ComboBoxEditField:
                    {
                        int frameWidth = pixelMetric(PM_ComboBoxFrameWidth, option, widget);
                        rect = comboBox->rect;
                        rect.setRight(rect.right() - rect.height());
                        rect.adjust(frameWidth, frameWidth, 0, -frameWidth);
                        break;
                    }

                    default:
                        break;
                }
            }
            break;
        }

        case CC_SpinBox:
        {
            if (auto spinBox = qstyleoption_cast<const QStyleOptionSpinBox*>(option))
            {
                int frame = spinBox->frame && spinBox->subControls.testFlag(SC_SpinBoxFrame) ? 1 : 0;
                QRect contentsRect = spinBox->rect.adjusted(frame, frame, -frame, -frame);

                switch (subControl)
                {
                    case SC_SpinBoxEditField:
                        return visualRect(spinBox->direction, contentsRect,
                            contentsRect.adjusted(0, 0, -Metrics::kSpinButtonWidth, 0));

                    case SC_SpinBoxDown:
                        return alignedRect(spinBox->direction, Qt::AlignRight | Qt::AlignBottom,
                            QSize(Metrics::kSpinButtonWidth, contentsRect.height() / 2), contentsRect);

                    case SC_SpinBoxUp:
                        return alignedRect(spinBox->direction, Qt::AlignRight | Qt::AlignTop,
                            QSize(Metrics::kSpinButtonWidth, contentsRect.height() / 2), contentsRect);

                    case SC_SpinBoxFrame:
                        return spinBox->rect;

                    default:
                        break;
                }
            }
            break;
        }

        case CC_GroupBox:
        {
            if (auto groupBox = qstyleoption_cast<const QStyleOptionGroupBox*>(option))
            {
                bool panel = groupBox->features.testFlag(QStyleOptionFrame::Flat);
                switch (subControl)
                {
                    case SC_GroupBoxFrame:
                    {
                        int topMargin = panel ? Metrics::kPanelHeaderHeight : Metrics::kGroupBoxTopMargin;
                        return option->rect.adjusted(0, topMargin - groupBox->lineWidth, 0, 0);
                    }

                    case SC_GroupBoxLabel:
                    {
                        if (groupBox->text.isEmpty() || !groupBox->subControls.testFlag(SC_GroupBoxLabel))
                            return QRect();

                        const int kTextFlags = Qt::AlignLeft | Qt::TextHideMnemonic;

                        if (panel)
                        {
                            QString text = groupBox->text;
                            QString detailText;

                            int splitPos = text.indexOf(QLatin1Char('\t'));
                            if (splitPos >= 0)
                            {
                                detailText = text.mid(splitPos + 1);
                                text = text.left(splitPos);
                            }

                            QFont font = widget ? widget->font() : QApplication::font();
                            font.setPixelSize(font.pixelSize() + 2);
                            font.setWeight(QFont::DemiBold);

                            int textWidth = QFontMetrics(font).width(text, -1, kTextFlags);

                            if (!detailText.isEmpty())
                            {
                                int detailWidth = groupBox->fontMetrics.width(detailText, -1, kTextFlags);
                                textWidth += Metrics::kStandardPadding + detailWidth;
                            }

                            return QRect(
                                option->rect.left(),
                                option->rect.top(),
                                textWidth,
                                Metrics::kPanelHeaderHeight);
                        }

                        return QRect(
                            option->rect.left() + Metrics::kGroupBoxContentMargins.left() - Metrics::kStandardPadding,
                            option->rect.top(),
                            groupBox->fontMetrics.width(groupBox->text, -1, kTextFlags) + Metrics::kStandardPadding * 2,
                            Metrics::kGroupBoxTopMargin * 2);
                    }

                    case SC_GroupBoxCheckBox:
                    {
                        if (panel)
                        {
                            //TODO: #vkutin Rewrite calculation without calling subControlRect(SC_GroupBoxLabel)
                            QRect boundRect = subControlRect(CC_GroupBox, option, SC_GroupBoxLabel, widget);
                            boundRect.setRight(option->rect.right());
                            rect = alignedRect(Qt::LeftToRight, Qt::AlignRight | Qt::AlignVCenter,
                                               Metrics::kStandaloneSwitchSize + kSwitchFocusFrameMargins, boundRect);
                        }
                        break;
                    }

                    case SC_GroupBoxContents:
                    {
                        if (panel)
                        {
                            return option->rect.adjusted(
                                Metrics::kPanelContentMargins.left(),
                                Metrics::kPanelContentMargins.top() + Metrics::kPanelHeaderHeight,
                               -Metrics::kPanelContentMargins.right(),
                               -Metrics::kPanelContentMargins.bottom());
                        }

                        return option->rect.adjusted(
                            Metrics::kGroupBoxContentMargins.left(),
                            Metrics::kGroupBoxContentMargins.top() + Metrics::kGroupBoxTopMargin,
                           -Metrics::kGroupBoxContentMargins.right(),
                           -Metrics::kGroupBoxContentMargins.bottom());
                    }

                    default:
                        break;
                }
            }
            break;
        }

        case CC_ScrollBar:
        {
            if (auto scrollBar = qstyleoption_cast<const QStyleOptionSlider*>(option))
            {
                /* For some reason QCommonStyle returns scrollbar subcontrol rects in local coordinates.
                 * Fix that: */
                rect.moveTopLeft(option->rect.topLeft() + rect.topLeft());

                const int w = pixelMetric(PM_ScrollBarExtent, option, widget);

                switch (subControl)
                {
                    case SC_ScrollBarAddLine:
                        if (scrollBar->orientation == Qt::Vertical)
                            rect.setTop(rect.bottom());
                        else
                            rect.setLeft(rect.right());
                        break;

                    case SC_ScrollBarSubLine:
                        if (scrollBar->orientation == Qt::Vertical)
                            rect.setBottom(rect.top());
                        else
                            rect.setRight(rect.left());
                        break;

                    case SC_ScrollBarGroove:
                        rect = scrollBar->rect;
                        break;

                    case SC_ScrollBarSlider:
                        if (scrollBar->orientation == Qt::Vertical)
                            rect.adjust(0, -w, 0, w);
                        else
                            rect.adjust(-w, 0, w, 0);
                        break;

                    case SC_ScrollBarAddPage:
                        if (scrollBar->orientation == Qt::Vertical)
                            rect.adjust(0, w, 0, 0);
                        else
                            rect.adjust(w, 0, 0, 0);
                        break;

                    case SC_ScrollBarSubPage:
                        if (scrollBar->orientation == Qt::Vertical)
                            rect.adjust(0, 0, 0, -w);
                        else
                            rect.adjust(0, 0, -w, 0);
                        break;

                    default:
                        break;
                }
            }
            break;
        }

        default:
            break;
    }

    return rect;
}

QRect QnNxStyle::subElementRect(
        QStyle::SubElement subElement,
        const QStyleOption *option,
        const QWidget *widget) const
{
    switch (subElement)
    {
        case SE_LineEditContents:
        {
            if (!widget || !widget->parent() || !qobject_cast<const QAbstractItemView*>(widget->parent()->parent()))
                //TODO #vkutin See why this ugly "dp(6)" is here and not somewhere else
                return base_type::subElementRect(subElement, option, widget).adjusted(dp(6), 0, 0, 0);
            break;
        }

        case SE_PushButtonLayoutItem:
        {
            if (auto buttonBox = qobject_cast<const QDialogButtonBox *>(widget))
            {
                if (const QWidget* parentWidget = buttonBox->parentWidget())
                {
                    if (parentWidget->isTopLevel() && parentWidget->layout())
                    {
                        QMargins margins = parentWidget->layout()->contentsMargins();
                        if (margins.isNull() && buttonBox->contentsMargins().isNull())
                        {
                            int margin = proxy()->pixelMetric(PM_DefaultTopLevelMargin);
                            return QnGeometry::dilated(option->rect, margin);
                        }
                    }
                }
            }
            break;
        }

        case SE_PushButtonFocusRect:
            return QnGeometry::eroded(option->rect, 1);

        case SE_ProgressBarGroove:
        {
            if (auto progressBar = qstyleoption_cast<const QStyleOptionProgressBar *>(option))
            {
                const bool hasText = progressBar->textVisible && !progressBar->text.isEmpty();
                const int kProgressBarWidth = dp(4);
                QSize size = progressBar->rect.size();

                if (progressBar->orientation == Qt::Horizontal)
                {
                    size.setHeight(kProgressBarWidth);
                    return alignedRect(progressBar->direction,
                                       hasText ? Qt::AlignBottom : Qt::AlignVCenter,
                                       size,
                                       progressBar->rect.adjusted(0, 0, 0, hasText ? -1 : 0));
                }
                else
                {
                    size.setWidth(kProgressBarWidth);
                    return alignedRect(progressBar->direction,
                                       hasText ? Qt::AlignLeft : Qt::AlignHCenter,
                                       size, progressBar->rect);
                }
            }
            break;
        }

        case SE_ProgressBarContents:
        {
            if (auto progressBar = qstyleoption_cast<const QStyleOptionProgressBar *>(option))
                return subElementRect(SE_ProgressBarGroove, progressBar, widget).adjusted(1, 1, -1, -1);

            break;
        }

        case SE_ProgressBarLabel:
        {
            if (auto progressBar = qstyleoption_cast<const QStyleOptionProgressBar *>(option))
            {
                const bool hasText = progressBar->textVisible && !progressBar->text.isEmpty();
                if (!hasText)
                    break;

                if (progressBar->orientation == Qt::Horizontal)
                    return progressBar->rect.adjusted(0, 0, 0, -dp(8));
                else
                    return progressBar->rect.adjusted(dp(8), 0, 0, 0);
            }
            break;
        }

        case SE_TabWidgetTabBar:
        {
            if (auto tabWidget = qobject_cast<const QTabWidget*>(widget))
            {
                int hspace = pixelMetric(PM_TabBarTabHSpace, option, widget) / 2;

                QRect rect = base_type::subElementRect(subElement, option, widget);

                if (tabShape(tabWidget->tabBar()) == TabShape::Compact)
                    rect.adjust(-hspace + kCompactTabFocusMargin, 0, -hspace + kCompactTabFocusMargin, 0);
                else
                    rect.adjust(hspace, 0, hspace, 0);

                if (rect.right() > option->rect.right())
                    rect.setRight(option->rect.right());

                return rect;
            }
            break;
        }

        case SE_TabBarTabLeftButton:
        {
            if (auto tabBar = qstyleoption_cast<const QStyleOptionTab*>(option))
            {
                QSize size = tabBar->leftButtonSize;
                size.setHeight(std::min(size.height(), tabBar->rect.height()));
                return aligned(size, tabBar->rect, Qt::AlignLeft | Qt::AlignVCenter);
            }
            break;
        }

        case SE_TabBarTabRightButton:
        {
            if (auto tabBar = qstyleoption_cast<const QStyleOptionTab*>(option))
            {
                QSize size = tabBar->rightButtonSize;
                size.setHeight(std::min(size.height(), tabBar->rect.height()));
                return aligned(size, tabBar->rect.adjusted(0, 0, -1, 0), Qt::AlignRight | Qt::AlignVCenter);
            }
            break;
        }

        case SE_ItemViewItemCheckIndicator:
        {
            if (auto item = qstyleoption_cast<const QStyleOptionViewItem *>(option))
            {
                /* Switch: */
                if (item->state.testFlag(State_On) || item->state.testFlag(State_Off))
                    return alignedRect(Qt::LeftToRight, Qt::AlignCenter, Metrics::kStandaloneSwitchSize + kSwitchFocusFrameMargins, option->rect);
            }
            /* FALL THROUGH */
        }

        case SE_ItemViewItemText:
        case SE_ItemViewItemDecoration:
        {
            if (auto item = qstyleoption_cast<const QStyleOptionViewItem*>(option))
            {
                int defaultMargin = pixelMetric(PM_FocusFrameHMargin, option, widget) + 1;
                QnIndentation indents = itemViewItemIndentation(item);

                QRect rect = item->rect.adjusted(indents.left() - defaultMargin, 0, -(indents.right() - defaultMargin), 0);

                /* Workaround to be able to align icon in an icon-only viewitem by model / Qt::TextAlignmentRole: */
                if (isIconOnlyItem(*item))
                {
                    QSize iconSize = item->icon.actualSize(item->decorationSize);
                    return alignedRect(item->direction, item->displayAlignment, iconSize, rect);
                }

                QStyleOptionViewItem newOpt(*item);
                newOpt.rect = rect;
                return base_type::subElementRect(subElement, &newOpt, widget);
            }
            break;
        }

        case SE_ItemViewItemFocusRect:
            return option->rect;

        case SE_HeaderArrow:
        {
            if (auto header = qstyleoption_cast<const QStyleOptionHeader *>(option))
            {
                QRect rect = header->rect.adjusted(Metrics::kStandardPadding, 0, -Metrics::kStandardPadding, 0);
                Qt::Alignment alignment = static_cast<Qt::Alignment>(styleHint(SH_Header_ArrowAlignment, header, widget));

                if (alignment.testFlag(Qt::AlignRight))
                {
                    QRect labelRect = subElementRect(SE_HeaderLabel, option, widget);
                    int margin = pixelMetric(PM_HeaderMargin, header, widget);
                    rect.setLeft(labelRect.right() + margin);
                }

                QSize size(Metrics::kSortIndicatorSize, Metrics::kSortIndicatorSize);
                return alignedRect(Qt::LeftToRight, Qt::AlignLeft | (alignment & Qt::AlignVertical_Mask), size, rect);
            }
            break;
        }

        case SE_HeaderLabel:
        {
            if (auto header = qstyleoption_cast<const QStyleOptionHeader *>(option))
            {
                QRect rect = header->rect.adjusted(Metrics::kStandardPadding, 0, -Metrics::kStandardPadding, 0);

                Qt::Alignment arrowAlignment =
                    static_cast<Qt::Alignment>(styleHint(SH_Header_ArrowAlignment, header, widget));
                if (arrowAlignment.testFlag(Qt::AlignLeft))
                {
                    int margin = pixelMetric(PM_HeaderMargin, header, widget);
                    int arrowSize = pixelMetric(PM_HeaderMarkSize, header, widget);
                    rect.setLeft(rect.left() + arrowSize + margin);
                }

                int flags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextHideMnemonic;
                rect = option->fontMetrics.boundingRect(rect, flags, header->text);
                return rect;
            }
            break;
        }
        default:
            break;
    } // switch

    return base_type::subElementRect(subElement, option, widget);
}

QSize QnNxStyle::sizeFromContents(
        ContentsType type,
        const QStyleOption *option,
        const QSize &size,
        const QWidget *widget) const
{
    switch (type)
    {
        case CT_CheckBox:
            return QSize(
                size.width() + proxy()->pixelMetric(PM_IndicatorWidth, option, widget) +
                               proxy()->pixelMetric(PM_CheckBoxLabelSpacing, option, widget),
                qMax(size.height(), proxy()->pixelMetric(PM_IndicatorHeight, option, widget)));

        case CT_RadioButton:
            return QSize(
                size.width() + proxy()->pixelMetric(PM_ExclusiveIndicatorWidth, option, widget) +
                               proxy()->pixelMetric(PM_RadioButtonLabelSpacing, option, widget),
                qMax(size.height(), proxy()->pixelMetric(PM_ExclusiveIndicatorHeight, option, widget)));

        case CT_PushButton:
        {
            QSize switchSize;
            if (isCheckableButton(option))
                switchSize = Metrics::kButtonSwitchSize + QSize(Metrics::kSwitchMargin, 0);

            return QSize(qMax(Metrics::kMinimumButtonWidth, size.width() + switchSize.width() + 2 * pixelMetric(PM_ButtonMargin, option, widget)),
                qMax(qMax(size.height(), switchSize.height()), Metrics::kButtonHeight));
        }

        case CT_LineEdit:
            return QSize(size.width(), qMax(size.height(), Metrics::kButtonHeight));

        case CT_SpinBox:
        {
            /* QDateTimeEdit uses CT_SpinBox style with combo (!) box subcontrols, so we have to handle this case separately: */
            if (qobject_cast<const QDateTimeEdit *>(widget))
            {
                const QStyleOptionSpinBox *spinBox = qstyleoption_cast<const QStyleOptionSpinBox *>(option);
                if (spinBox && spinBox->subControls.testFlag(SC_ComboBoxArrow))
                {
                    int hMargin = pixelMetric(PM_ButtonMargin, option, widget);
                    int height = qMax(size.height(), Metrics::kButtonHeight);
                    int width = qMax(Metrics::kMinimumButtonWidth, size.width() + hMargin + height);
                    return QSize(width, height);
                }
            }

            /* Processing for normal spin boxes: */
            return size + QSize(Metrics::kStandardPadding * 2 + Metrics::kSpinButtonWidth, 0);
        }

        case CT_ComboBox:
        {
            bool hasArrow = false;
            if (auto comboBox = qstyleoption_cast<const QStyleOptionComboBox *>(option))
                hasArrow = comboBox->subControls.testFlag(SC_ComboBoxArrow);

            int height = qMax(size.height(), Metrics::kButtonHeight);

            int hMargin = pixelMetric(PM_ButtonMargin, option, widget);
            int width = qMax(Metrics::kMinimumButtonWidth,
                             size.width() + hMargin + (hasArrow ? height : hMargin));

            return QSize(width, height);
        }

        case CT_GroupBox:
        {
            if (auto groupBox = qstyleoption_cast<const QStyleOptionGroupBox*>(option))
            {
                const bool panel = groupBox->features.testFlag(QStyleOptionFrame::Flat);
                if (panel)
                {
                    return size + QSize(
                        Metrics::kPanelContentMargins.left() + Metrics::kPanelContentMargins.right(),
                        Metrics::kPanelContentMargins.top() + Metrics::kPanelContentMargins.bottom() + Metrics::kPanelHeaderHeight);
                }
                else
                {
                    return size + QSize(
                        Metrics::kGroupBoxContentMargins.left() + Metrics::kGroupBoxContentMargins.right(),
                        Metrics::kGroupBoxContentMargins.top() + Metrics::kGroupBoxContentMargins.bottom() + Metrics::kGroupBoxTopMargin);
                }
            }

            break;
        }

        case CT_TabBarTab:
        {
            if (auto tab = qstyleoption_cast<const QStyleOptionTab*>(option))
            {
                TabShape shape = tabShape(widget);

                const int kRoundedSize = dp(36) + dp(1); /* shadow */
                const int kCompactSize = dp(32);

                int width = size.width();
                int height = size.height();

                if (shape != TabShape::Rectangular)
                    height = qMax(height, (shape == TabShape::Default) ? kRoundedSize : kCompactSize);

                if (widget)
                    height = qMin(height, widget->maximumHeight());

                return QSize(width, height);
            }
            break;
        }

        case CT_MenuItem:
            return QSize(size.width() + dp(24) + 2 * Metrics::kMenuItemHPadding,
                         size.height() + 2 * Metrics::kMenuItemVPadding);

        case CT_HeaderSection:
        {
            if (auto header = qstyleoption_cast<const QStyleOptionHeader *>(option))
            {
                QSize textSize = header->fontMetrics.size(Qt::TextHideMnemonic, header->text);

                int width = textSize.width();

                width += 2 * Metrics::kStandardPadding;

                if (header->sortIndicator != QStyleOptionHeader::None)
                {
                    width += pixelMetric(PM_HeaderMargin, header, widget);
                    width += pixelMetric(PM_HeaderMarkSize, header, widget);
                }

                return QSize(width, qMax(textSize.height(), Metrics::kHeaderSize));
            }
            break;
        }

        case CT_ItemViewItem:
        {
            if (const QStyleOptionViewItem *item = qstyleoption_cast<const QStyleOptionViewItem *>(option))
            {
                QnIndentation indents = itemViewItemIndentation(item);

                if (isCheckboxOnlyItem(*item))
                    return QSize(indents.left() + indents.right() + Metrics::kCheckIndicatorSize, Metrics::kCheckIndicatorSize);

                QSize sz = base_type::sizeFromContents(type, option, size, widget);
                sz.setHeight(qMax(sz.height(), Metrics::kViewRowHeight));
                    sz.setWidth(sz.width() + indents.left() + indents.right() - (pixelMetric(PM_FocusFrameHMargin, option, widget) + 1) * 2);
                return sz;
            }

            break;
        }

        case CT_ProgressBar:
        {
            if (auto progressBar = qstyleoption_cast<const QStyleOptionProgressBar*>(option))
            {
                bool hasText = progressBar->textVisible && !progressBar->text.isEmpty();
                if (!hasText)
                {
                    QStyleOptionProgressBar subOption(*progressBar);
                    subOption.rect.setSize(size);
                    return subElementRect(SE_ProgressBarGroove, &subOption, widget).size();
                }
            }
            break;
        }

        default:
            break;
    }

    return base_type::sizeFromContents(type, option, size, widget);
}

int QnNxStyle::pixelMetric(
        PixelMetric metric,
        const QStyleOption *option,
        const QWidget *widget) const
{
    switch (metric)
    {
        case PM_ButtonMargin:
            return dp(16);

        case PM_ButtonShiftVertical:
        case PM_ButtonShiftHorizontal:
        case PM_TabBarTabShiftVertical:
        case PM_TabBarTabShiftHorizontal:
            return 0;

        case PM_DefaultFrameWidth:
            return 0;

        case PM_DefaultTopLevelMargin:
            return Metrics::kDefaultTopLevelMargin;
        case PM_DefaultChildMargin:
            return Metrics::kDefaultChildMargin;

        case PM_ExclusiveIndicatorWidth:
        case PM_ExclusiveIndicatorHeight:
            return Metrics::kExclusiveIndicatorSize;
        case PM_IndicatorWidth:
        case PM_IndicatorHeight:
            return Metrics::kCheckIndicatorSize;

        case PM_FocusFrameHMargin:
        case PM_FocusFrameVMargin:
            return dp(1);

        case PM_HeaderDefaultSectionSizeVertical:
            return Metrics::kViewRowHeight;
        case PM_HeaderMarkSize:
            return Metrics::kSortIndicatorSize;
        case PM_HeaderMargin:
            return dp(6);

        case PM_LayoutBottomMargin:
        case PM_LayoutLeftMargin:
        case PM_LayoutRightMargin:
        case PM_LayoutTopMargin:
            if (qobject_cast<const QnAbstractPreferencesWidget*>(widget))
                return proxy()->pixelMetric(PM_DefaultTopLevelMargin);
            if (qobject_cast<const QnGenericTabbedDialog*>(widget))
                return 0;
            return base_type::pixelMetric(metric, option, widget);

        case PM_LayoutHorizontalSpacing:
            return Metrics::kDefaultLayoutSpacing.width();
        case PM_LayoutVerticalSpacing:
            return (qobject_cast<const QnGenericTabbedDialog*>(widget) == nullptr) ? Metrics::kDefaultLayoutSpacing.height() : 0;

        case PM_MenuVMargin:
            return dp(2);
        case PM_SubMenuOverlap:
            return dp(1);

        case PM_SliderControlThickness:
            return dp(16);
        case PM_SliderThickness:
            return dp(20);
        case PM_SliderLength:
        {
            if (option && option->styleObject)
            {
                bool ok(false);
                int result = option->styleObject->property(Properties::kSliderLength).toInt(&ok);
                if (ok && result >= 0)
                    return result;
            }
            return dp(16);
        }

        case PM_ScrollBarExtent:
            return dp(8);
        case PM_ScrollBarSliderMin:
            return dp(8);

        case PM_SplitterWidth:
            return dp(1);

        case PM_TabBarTabHSpace:
        case PM_TabBarTabVSpace:
            return tabShape(widget) == TabShape::Rectangular ? dp(8) : dp(20);

        case PM_TabBarScrollButtonWidth:
            return dp(24);

        case PM_TabCloseIndicatorWidth:
        case PM_TabCloseIndicatorHeight:
            return dp(24);

        case PM_ToolBarIconSize:
            return dp(32);

        default:
            break;
    }

    return base_type::pixelMetric(metric, option, widget);
}

int QnNxStyle::styleHint(
        StyleHint sh,
        const QStyleOption *option,
        const QWidget *widget,
        QStyleHintReturn *shret) const
{
    switch (sh)
    {
        case SH_GroupBox_TextLabelColor:
        {
            if (auto groupBox = qstyleoption_cast<const QStyleOptionGroupBox*>(option))
            {
                if (groupBox->features & QStyleOptionFrame::Flat)
                    return int(groupBox->palette.color(QPalette::Text).rgba());
                else
                    return int(groupBox->palette.color(QPalette::WindowText).rgba());
            }
            break;
        }

        case SH_Menu_MouseTracking:
        case SH_ComboBox_ListMouseTracking:
            return 1;
        case SH_ComboBox_PopupFrameStyle:
            return QFrame::StyledPanel;
        case QStyle::SH_DialogButtonBox_ButtonsHaveIcons:
            return 0;
        case SH_Slider_AbsoluteSetButtons:
            return Qt::LeftButton;
        case SH_Slider_StopMouseOverSlider:
            return true;
        case SH_FormLayoutLabelAlignment:
            return Qt::AlignRight | Qt::AlignVCenter;
        case SH_FormLayoutFormAlignment:
            return Qt::AlignLeft | Qt::AlignVCenter;
        case SH_FormLayoutFieldGrowthPolicy:
            return QFormLayout::AllNonFixedFieldsGrow;
        case SH_ItemView_ShowDecorationSelected:
            return 1;
        case SH_ItemView_ActivateItemOnSingleClick:
            return 0;
        case SH_Header_ArrowAlignment:
            return Qt::AlignRight | Qt::AlignVCenter;
        case SH_FocusFrame_AboveWidget:
            return 1;
        case SH_DialogButtonLayout:
            return QDialogButtonBox::KdeLayout;
        default:
            break;
    }

    return base_type::styleHint(sh, option, widget, shret);
}

QPixmap QnNxStyle::standardPixmap(StandardPixmap iconId, const QStyleOption* option, const QWidget* widget) const
{
    switch (iconId)
    {
        case SP_LineEditClearButton:
        {
            const int kQLineEditButtonSize = 16;
            return qnSkin->icon("theme/input_clear.png").pixmap(kQLineEditButtonSize, kQLineEditButtonSize);
        }

        default:
            return base_type::standardPixmap(iconId, option, widget);
    }
}

void QnNxStyle::polish(QWidget *widget)
{
    base_type::polish(widget);

    if (qobject_cast<QAbstractSpinBox*>(widget) ||
        qobject_cast<QAbstractButton*>(widget) ||
        qobject_cast<QAbstractSlider*>(widget) ||
        qobject_cast<QGroupBox*>(widget))
    {
        widget->setAttribute(Qt::WA_Hover);
    }

    if (qobject_cast<QPushButton*>(widget) ||
        qobject_cast<QToolButton*>(widget))
    {
        if (!widget->property(Properties::kDontPolishFontProperty).toBool())
        {
            QFont font = widget->font();
            font.setWeight(QFont::DemiBold);
            widget->setFont(font);
        }
    }

    if (qobject_cast<QHeaderView*>(widget))
    {
        if (!widget->property(Properties::kDontPolishFontProperty).toBool())
        {
            QFont font = widget->font();
            font.setWeight(QFont::DemiBold);
            font.setPixelSize(dp(14));
            widget->setFont(font);
        }
        widget->setAttribute(Qt::WA_Hover);
    }

    if (auto lineEdit = qobject_cast<QLineEdit*>(widget))
    {
        if (!widget->property(Properties::kDontPolishFontProperty).toBool() && !isItemViewEdit(widget))
        {
            QFont font = widget->font();
            font.setPixelSize(dp(14));
            widget->setFont(font);
        }
    }

    if (qobject_cast<QPlainTextEdit*>(widget) || qobject_cast<QTextEdit*>(widget))
    {
        /*
         * There are only three ways to change content margins of QPlainTextEdit or QTextEdit without subclassing:
         *  - use QTextDocument::setDocumentMargin(), but that margin is uniform
         *  - use Qt stylesheets, but they're slow and it's not a good idea to change stylesheet in polish()
         *  - hack access to protected method QAbstractScrollArea::setViewportMargins()
         */
        ViewportMarginsAccessHack* area = static_cast<ViewportMarginsAccessHack*>(static_cast<QAbstractScrollArea*>(widget));

        QnTypedPropertyBackup<QMargins, ViewportMarginsAccessHack>::backup(area, &ViewportMarginsAccessHack::viewportMargins,
            QN_SETTER(ViewportMarginsAccessHack::setViewportMargins), kViewportMarginsBackupId);

        const int kTextDocumentDefaultMargin = 4; // in Qt
        int h = style::Metrics::kStandardPadding - kTextDocumentDefaultMargin;
        int v = dp(6) - kTextDocumentDefaultMargin;
        area->setViewportMargins(QMargins(h, v, h, v));

        if (!widget->property(Properties::kDontPolishFontProperty).toBool())
        {
            QFont font = widget->font();
            font.setPixelSize(dp(14));
            widget->setFont(font);
        }
    }

    if (auto comboBox = qobject_cast<QComboBox*>(widget))
    {
        comboBox->setAttribute(Qt::WA_Hover);

        static const QByteArray kDefaultDelegateClassName("QComboBoxDelegate");
        if (comboBox->itemDelegate()->metaObject()->className() == kDefaultDelegateClassName)
        {
            auto getDelegateClass = [](const QComboBox* comboBox)
            {
                return comboBox->itemDelegate()->metaObject();
            };

            auto setDelegateClass = [](QComboBox* comboBox, const QMetaObject* delegateClass)
            {
                comboBox->setItemDelegate(static_cast<QAbstractItemDelegate*>(delegateClass->newInstance()));
            };

            QnTypedPropertyBackup<const QMetaObject*, QComboBox>::backup(comboBox, getDelegateClass, setDelegateClass, kDelegateClassBackupId);
            comboBox->setItemDelegate(new QnStyledComboBoxDelegate());
        }
    }

    if (qobject_cast<QScrollBar*>(widget))
    {
        /* In certain containers we want scrollbars with transparent groove: */
        widget->setAttribute(Qt::WA_OpaquePaintEvent, false);
        widget->setAttribute(Qt::WA_Hover);
    }

    if (qobject_cast<QTabBar*>(widget))
    {
        if (!widget->property(Properties::kDontPolishFontProperty).toBool())
        {
            QFont font = widget->font();
            font.setPixelSize(dp(12));
            if (tabShape(widget) == TabShape::Rectangular)
                font.setWeight(QFont::DemiBold);
            widget->setFont(font);
        }

        /* Install hover events tracking: */
        widget->installEventFilter(this);
        widget->setAttribute(Qt::WA_Hover);
    }

    if (qobject_cast<QAbstractButton*>(widget))
    {
        /* Install event filter to process wheel events: */
        widget->installEventFilter(this);
    }

    if (auto button = qobject_cast<QToolButton*>(widget))
    {
        /* Left scroll button in a tab bar: add shadow effect: */
        if (button->arrowType() == Qt::LeftArrow && isWidgetOwnedBy<QTabBar>(button))
        {
            auto effect = new QGraphicsDropShadowEffect(button);
            effect->setXOffset(-dp(4.0));
            effect->setYOffset(0);

            QColor shadowColor = mainColor(Colors::kBase);
            shadowColor.setAlphaF(0.5);
            effect->setColor(shadowColor);

            button->setGraphicsEffect(effect);
        }
    }

    QWidget* popupToCustomizeShadow = nullptr;

    if (qobject_cast<QMenu*>(widget))
    {
        widget->setAttribute(Qt::WA_TranslucentBackground);
#ifdef Q_OS_WIN
        widget->setWindowFlags(widget->windowFlags() | Qt::FramelessWindowHint);
#endif
        popupToCustomizeShadow = widget;
    }

    if (auto view = qobject_cast<QAbstractItemView*>(widget))
    {
        view->viewport()->setAttribute(Qt::WA_Hover);
        QWidget* topLevel = view->window();

        if (isWidgetOwnedBy<QComboBox>(topLevel))
        {
            /* Set margins for drop-down list container: */
            topLevel->setContentsMargins(0, 2, 0, 2);
            popupToCustomizeShadow = topLevel;
        }
    }

    /*
     * Insert horizontal separator line into QInputDialog above its button box.
     * Since input dialogs are short-lived, don't bother with unpolishing.
     */
    if (auto inputDialog = qobject_cast<QInputDialog*>(widget))
    {
        Q_D(const QnNxStyle);
        d->polishInputDialog(inputDialog);
    }

    if (auto label = qobject_cast<QLabel*>(widget))
        QnObjectCompanion<QnLinkHoverProcessor>::installUnique(label, kLinkHoverProcessorCompanion);

#ifdef CUSTOMIZE_POPUP_SHADOWS
    if (popupToCustomizeShadow)
    {
        /* Create customized shadow: */
        if (auto shadow = QnObjectCompanion<QnPopupShadow>::installUnique(popupToCustomizeShadow, kPopupShadowCompanion))
        {
            QnPaletteColor shadowColor = mainColor(Colors::kBase).darker(3);
            shadowColor.setAlphaF(0.5);
            shadow->setColor(shadowColor);
            shadow->setOffset(0, 10);
            shadow->setBlurRadius(20);
            shadow->setSpread(0);
        }
    }
#endif
}

void QnNxStyle::unpolish(QWidget* widget)
{
    if (qobject_cast<QAbstractButton*>(widget) ||
        qobject_cast<QHeaderView*>(widget) ||
        qobject_cast<QLineEdit*>(widget) ||
        qobject_cast<QTabBar*>(widget))
    {
        if (!widget->property(Properties::kDontPolishFontProperty).toBool())
            widget->setFont(qApp->font());
    }

    if (auto comboBox = qobject_cast<QComboBox*>(widget))
        QnAbstractPropertyBackup::restore(comboBox, kDelegateClassBackupId);

    if (qobject_cast<QPlainTextEdit*>(widget) || qobject_cast<QTextEdit*>(widget))
        QnAbstractPropertyBackup::restore(widget, kViewportMarginsBackupId);

    QWidget* popupWithCustomizedShadow = nullptr;

    if (auto menu = qobject_cast<QMenu*>(widget))
        popupWithCustomizedShadow = menu;

    if (auto view = qobject_cast<QAbstractItemView*>(widget))
    {
        if (isWidgetOwnedBy<QComboBox>(view))
        {
            /* Reset margins for drop-down list container: */
            QWidget* parentWidget = view->parentWidget();
            parentWidget->setContentsMargins(0, 0, 0, 0);
            popupWithCustomizedShadow = parentWidget;
        }
    }

#ifdef CUSTOMIZE_POPUP_SHADOWS
    if (popupWithCustomizedShadow)
        QnObjectCompanionManager::uninstall(popupWithCustomizedShadow, kPopupShadowCompanion);
#endif

    if (auto label = qobject_cast<QLabel*>(widget))
        QnObjectCompanionManager::uninstall(label, kLinkHoverProcessorCompanion);

    if (auto tabBar = qobject_cast<QTabBar*>(widget))
    {
        /* Remove hover events tracking: */
        tabBar->setProperty(kHoveredChildProperty, QVariant());
        tabBar->removeEventFilter(this);
    }

    if (qobject_cast<QAbstractButton*>(widget))
        widget->removeEventFilter(this);

    if (auto button = qobject_cast<QToolButton*>(widget))
    {
        /* Left scroll button in a tab bar: remove shadow effect: */
        if (button->arrowType() == Qt::LeftArrow && isWidgetOwnedBy<QTabBar>(button))
            button->setGraphicsEffect(nullptr);
    }

    base_type::unpolish(widget);
}

QnNxStyle *QnNxStyle::instance()
{
    QStyle *style = qApp->style();
    if (QProxyStyle *proxyStyle = qobject_cast<QProxyStyle *>(style))
        style = proxyStyle->baseStyle();

    return qobject_cast<QnNxStyle *>(style);
}

QString QnNxStyle::Colors::paletteName(QnNxStyle::Colors::Palette palette)
{
    switch (palette)
    {
        case kBase:
            return lit("dark");
        case kContrast:
            return lit("light");
        case kBlue:
            return lit("blue");
        case kGreen:
            return lit("green");
        case kBrand:
            return lit("brand");
        case kRed:
            return lit("red");
        case kYellow:
            return lit("yellow");
    }

    return QString();
}

bool QnNxStyle::eventFilter(QObject* object, QEvent* event)
{
    /* QTabBar marks a tab as hovered even if a child widget is hovered above that tab.
     * To overcome this problem we process hover events and track hovered children: */
    if (auto tabBar = qobject_cast<QTabBar*>(object))
    {
        QWidget* hoveredChild = nullptr;
        switch (event->type())
        {
            case QEvent::HoverEnter:
            case QEvent::HoverMove:
                hoveredChild = tabBar->childAt(static_cast<QHoverEvent*>(event)->pos());
                /* FALL THROUGH */
            case QEvent::HoverLeave:
            {
                if (tabBar->property(kHoveredChildProperty).value<QWidget*>() != hoveredChild)
                {
                    tabBar->setProperty(kHoveredChildProperty, QVariant::fromValue(hoveredChild));
                    tabBar->update();
                }
                break;
            }
        }
    }
    /* Disabled QAbstractButton eats mouse wheel events.
     * Here we correct this: */
    else if (auto button = qobject_cast<QAbstractButton*>(object))
    {
        if (!button->isEnabled() && event->type() == QEvent::Wheel)
        {
            event->ignore();
            return true;
        }
    }

    return base_type::eventFilter(object, event);
}
