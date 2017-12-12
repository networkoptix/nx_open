#include "nx_style.h"
#include "nx_style_p.h"
#include "globals.h"
#include "skin.h"

#include <cmath>

#include <QtCore/QtMath>
#include <QtGui/QPainter>
#include <QtGui/QWindow>
#include <QtGui/private/qfont_p.h>

#include <QtWidgets/QApplication>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QCalendarWidget>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDateTimeEdit>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QItemDelegate>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QProxyStyle>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QStyleOptionButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeView>

#include <QtWidgets/private/qabstractitemview_p.h>

#include <nx/client/desktop/ui/common/detail/base_input_field.h>
#include <nx/client/desktop/ui/common/painter_transform_scale_stripper.h>

#include <ui/common/indents.h>
#include <ui/common/popup_shadow.h>
#include <ui/common/link_hover_processor.h>
#include <ui/delegates/styled_combo_box_delegate.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/widgets/common/input_field.h>
#include <ui/widgets/common/scroll_bar_proxy.h>
#include <ui/widgets/calendar_widget.h>

#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>
#include <utils/common/object_companion.h>
#include <utils/common/property_backup.h>
#include <utils/common/scoped_painter_rollback.h>

#include <utils/math/color_transformations.h>
#include <nx/utils/string.h>
#include <nx/utils/math/fuzzy.h>


using namespace style;
using namespace nx::client::desktop::ui;

namespace
{
    constexpr bool kCustomizePopupShadows = true;

#if defined(Q_OS_WIN) || defined(Q_OS_OSX)
    constexpr bool kForceMenuMouseReplay = true;
#else
    constexpr bool kForceMenuMouseReplay = false;
#endif

    const char* kDelegateClassBackupId = "delegateClass";
    const char* kViewportMarginsBackupId = "viewportMargins";
    const char* kContentsMarginsBackupId = "contentsMargins";

    const char* kHoveredWidgetProperty = "_qn_hoveredWidget"; // QPointer<QWidget>

    const QSize kSwitchFocusFrameMargins = QSize(4, 4); // 2 at left, 2 at right, 2 at top, 2 at bottom

    const char* kPopupShadowCompanion = "popupShadow";

    const char* kLinkHoverProcessorCompanion = "linkHoverProcessor";

    const char* kCalendarDelegateCompanion = "calendarDelegateReplacement";

    const int kQtHeaderIconMargin = 2; // a margin between item view header's icon and label used by Qt

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

        QPen pen(color, width);
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

        QPen pen(color, 1.5);
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

    bool isAccented(const QWidget* widget)
    {
        return widget && widget->property(Properties::kAccentStyleProperty).toBool();
    }

    bool isWarningStyle(const QWidget* widget)
    {
        return widget && widget->property(Properties::kWarningStyleProperty).toBool();
    }

    bool isSwitchButtonCheckbox(const QWidget* widget)
    {
        return widget && widget->property(Properties::kCheckBoxAsButton).toBool();
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

    QMargins groupBoxContentsMargins(bool panel, const QWidget* widget)
    {
        QMargins margins = panel
            ? Metrics::kPanelContentMargins
            : Metrics::kGroupBoxContentMargins;

        if (widget)
        {
            bool ok = false;
            const int top = widget->property(Properties::kGroupBoxContentTopMargin).toInt(&ok);
            if (ok)
                margins.setTop(top);
        }

        margins.setTop(margins.top() + (panel
            ? Metrics::kPanelHeaderHeight
            : Metrics::kGroupBoxTopMargin));

        return margins;
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

        if (!widget)
            return true;

        /* QTabBar marks a tab as hovered even if a child widget is hovered above that tab.
         * To overcome this problem we process hover events and track hovered children: */
        auto hoveredWidget = widget->property(kHoveredWidgetProperty).value<QPointer<QWidget>>();
        if (!hoveredWidget)
            return false;

        if (hoveredWidget == widget)
            return true;

        /* Make exception for close buttons: */
        static const QByteArray kQTabBarCloseButtonClassName = "CloseButton";
        if (hoveredWidget->metaObject()->className() == kQTabBarCloseButtonClassName)
            return true;

        return false;
    }

    QIcon::Mode buttonIconMode(const QStyleOption& option)
    {
        if (!option.state.testFlag(QStyle::State_Enabled))
            return QIcon::Disabled;
        if (option.state.testFlag(QStyle::State_Sunken))
            return QnIcon::Pressed;
        if (option.state.testFlag(QStyle::State_MouseOver))
            return QIcon::Active;

        return QIcon::Normal;
    }

    bool isNonEditableComboBox(const QWidget* widget)
    {
        auto comboBox = qobject_cast<const QComboBox*>(widget);
        return comboBox && !comboBox->isEditable();
    }

    QnIndents itemViewItemIndents(const QStyleOptionViewItem* item)
    {
        static const QnIndents kDefaultIndents(Metrics::kStandardPadding, Metrics::kStandardPadding);

        auto view = qobject_cast<const QAbstractItemView*>(item->widget);
        if (!view)
            return kDefaultIndents;

        if (item->viewItemPosition == QStyleOptionViewItem::Middle)
            return kDefaultIndents;

        QVariant value = view->property(Properties::kSideIndentation);
        if (!value.canConvert<QnIndents>())
            return kDefaultIndents;

        QnIndents Indents = value.value<QnIndents>();
        switch (item->viewItemPosition)
        {
            case QStyleOptionViewItem::Beginning:
                Indents.setRight(kDefaultIndents.right());
                break;

            case QStyleOptionViewItem::End:
                Indents.setLeft(kDefaultIndents.left());
                break;

            case QStyleOptionViewItem::Invalid:
            {
                const QModelIndex& index = item->index;
                if (!index.isValid())
                    return kDefaultIndents;

                struct VisibilityChecker : public QAbstractItemView
                {
                    using QAbstractItemView::isIndexHidden;
                };

                auto checker = static_cast<const VisibilityChecker*>(view);

                for (int column = index.column() - 1; column >= 0; --column)
                {
                    if (!checker->isIndexHidden(index.sibling(index.row(), column)))
                    {
                        Indents.setLeft(kDefaultIndents.left());
                        break;
                    }
                }

                int columnCount = view->model()->columnCount(view->rootIndex());
                for (int column = index.column() + 1; column < columnCount; ++column)
                {
                    if (!checker->isIndexHidden(index.sibling(index.row(), column)))
                    {
                        Indents.setRight(kDefaultIndents.right());
                        break;
                    }
                }

                break;
            }

            case QStyleOptionViewItem::OnlyOne:
            default:
                break;
        }

        return Indents;
    }

    template <class T>
    T* isWidgetOwnedBy(const QWidget* widget)
    {
        if (!widget)
            return nullptr;

        for (QWidget* parent = widget->parentWidget(); parent != nullptr; parent = parent->parentWidget())
        {
            if (auto desiredParent = qobject_cast<T*>(parent))
                return desiredParent;
        }

        return nullptr;
    }

    bool isIconListCombo(const QComboBox* combo)
    {
        if (!combo)
            return false;

        const auto list = qobject_cast<const QListView*>(combo->view());
        return list && list->viewMode() == QListView::IconMode;
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

    /*
     * Replacement for standard QCalendarWidget item delegate.
     * Installs itself in constructor, restores previous delegate in destructor.
     */
    class CalendarDelegateReplacement: public QItemDelegate
    {
    public:
        CalendarDelegateReplacement(QAbstractItemView* view, QCalendarWidget* parent):
            QItemDelegate(parent),
            m_view(view),
            m_previous(view ? view->itemDelegate() : nullptr)
        {
            m_view->setItemDelegate(this);
        }

        virtual ~CalendarDelegateReplacement()
        {
            if (m_view && m_previous)
                m_view->setItemDelegate(m_previous);
        }

        virtual void paint(QPainter* painter, const QStyleOptionViewItem& option,
            const QModelIndex& index) const override
        {
            QStyleOptionViewItem viewOption(option);
            viewOption.state &= ~QStyle::State_HasFocus; //< don't draw focus rect
            viewOption.palette.setColor(QPalette::Highlight, Qt::transparent);

            const bool selected = option.showDecorationSelected
                && (option.state.testFlag(QStyle::State_Selected));

            const bool inaccessible = index.flags() == 0;

            const bool hovered = option.state.testFlag(QStyle::State_MouseOver)
                && !inaccessible && m_view && m_view->isEnabled();

            /*
            * Standard QCalendarWidget does not support hover.
            * So we draw cells background here ourselves and don't forget to customize
            * calendar view's QPalette::Base and QPalette::Window as fully transparent
            * to prevent standard delegate from overpainting our background.
            */
            painter->fillRect(option.rect, option.palette.color(hovered
                ? QPalette::Midlight
                : QPalette::Mid));

            if (selected)
            {
                static const qreal kSelectionBackgroundOpacity = 0.2;
                static const qreal kFrameWidth = 2.0;

                const auto color = option.palette.color(QPalette::Highlight);

                QnScopedPainterPenRollback penRollback(painter,
                    QPen(color, kFrameWidth, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));

                QnScopedPainterBrushRollback brushRollback(painter,
                    toTransparent(color, kSelectionBackgroundOpacity));

                static const auto kShift = kFrameWidth / 2.0;
                painter->drawRect(QRectF(option.rect).adjusted(
                    kShift, kShift, -kShift - 1, -kShift - 1));
            }

            QnScopedPainterOpacityRollback opacityRollback(painter);
            if (inaccessible)
            {
                /* Paint semi-transparent highlighted foreground: */
                painter->setOpacity(Hints::kDisabledItemOpacity);
                viewOption.state |= QStyle::State_Selected;
                viewOption.showDecorationSelected = true;
            }

            if (m_previous)
                m_previous->paint(painter, viewOption, index);

            opacityRollback.rollback();

            /* Paint shadow under header, if needed: */
            if (auto calendar = qobject_cast<QCalendarWidget*>(parent()))
            {
                static const int kHeaderRow = 0;
                if (calendar->horizontalHeaderFormat() != QCalendarWidget::NoHorizontalHeader
                    && index.row() == kHeaderRow)
                {
                    QnScopedPainterPenRollback penRollback(painter,
                        option.palette.color(QPalette::Shadow));

                    painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
                }
            }
        }

    private:
        QPointer<QAbstractItemView> m_view;
        QPointer<QAbstractItemDelegate> m_previous;
    };

} // unnamed namespace

QnNxStyle::QnNxStyle() :
    base_type(*(new QnNxStylePrivate()))
{
    // TODO: Think through how to make it better
    /* Temporary fix for graphics items not receiving ungrabMouse when graphics view deactivates.
     * Menu popups do not cause deactivation but steal focus so we should handle that too. */
    installEventHandler(qApp, { QEvent::WindowDeactivate, QEvent::FocusOut }, this,
        [this](QObject* watched, QEvent*)
        {
            auto view = qobject_cast<QGraphicsView*>(watched);
            if (!view || !view->scene())
                return;

            while (auto grabber = view->scene()->mouseGrabberItem())
                grabber->ungrabMouse();
        });

    /* Windows-style handling of mouse clicks outside of popup menu. */
    //QTBUG: Qt is supposed to handle this, but currently it seems broken.
    if (kForceMenuMouseReplay)
    {
        installEventHandler(qApp, QEvent::MouseButtonPress, this,
            [this](QObject* watched, QEvent* event)
            {
                if (!event->spontaneous())
                    return;

                auto activeMenu = qobject_cast<QMenu*>(qApp->activePopupWidget());
                if (activeMenu != watched)
                    return;

                auto mouseEvent = static_cast<QMouseEvent*>(event);
                auto globalPos = mouseEvent->globalPos();

                if (activeMenu->geometry().contains(globalPos))
                    return;

                /* If menu was invoked by a click on some area we most probably want to
                 * prevent re-invoking menu if it was closed by click in the same area: */
                QRect noReplayRect = activeMenu->property(Properties::kMenuNoMouseReplayRect).value<QRect>();
                if (noReplayRect.isValid() && noReplayRect.contains(globalPos))
                    return;

                auto window = QGuiApplication::topLevelAt(globalPos);
                if (!window || window->flags().testFlag(Qt::Popup))
                    return;

                auto localPos = window->mapFromGlobal(globalPos);

                qApp->postEvent(window, new QMouseEvent(QEvent::MouseButtonPress,
                    localPos, globalPos, mouseEvent->button(),
                    mouseEvent->buttons(), mouseEvent->modifiers()), Qt::HighEventPriority);
            });
    }
}

QnNxStyle::~QnNxStyle()
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

            if (qobject_cast<QAbstractItemView*>(option->styleObject))
                return;

            QColor color = isAccented(widget) || isWarningStyle(widget)
                ? option->palette.color(QPalette::BrightText)
                : option->palette.color(QPalette::Highlight);
            color.setAlphaF(0.5);

            QnScopedPainterPenRollback penRollback(painter, QPen(color, 0, Qt::DotLine));
            QnScopedPainterBrushRollback brushRollback(painter, Qt::NoBrush);
            QnScopedPainterAntialiasingRollback aaRollback(painter, false);

            painter->drawRoundedRect(QnGeometry::eroded(QRectF(option->rect), 0.5), 1.0, 1.0);
            return;
        }

        case PE_PanelButtonCommand:
        {
            const bool enabled = option->state.testFlag(State_Enabled);
            const bool pressed = enabled && option->state.testFlag(State_Sunken);
            const bool hovered = enabled && option->state.testFlag(State_MouseOver);

            QnPaletteColor mainColor = findColor(option->palette.button().color());

            if (isWarningStyle(widget))
            {
                mainColor = this->mainColor(Colors::kRed);
                if (!enabled)
                    mainColor.setAlphaF(style::Hints::kDisabledBrandedButtonOpacity);
            }
            else if (isAccented(widget))
            {
                mainColor = this->mainColor(Colors::kBrand);
                if (!enabled)
                    mainColor.setAlphaF(style::Hints::kDisabledBrandedButtonOpacity);
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
            const bool enabled = option->state.testFlag(State_Enabled);
            const bool hovered = option->state.testFlag(State_MouseOver) && enabled;

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

            if (isWarningStyle(widget))
            {
                mainColor = this->mainColor(Colors::kRed);
                if (!enabled)
                    mainColor.setAlphaF(style::Hints::kDisabledItemOpacity);
            }
            else if (isAccented(widget))
            {
                mainColor = this->mainColor(Colors::kBrand);
                if (!enabled)
                    mainColor.setAlphaF(style::Hints::kDisabledItemOpacity);
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
                using InputField = nx::client::desktop::ui::detail::BaseInputField;
                if (auto inputTextField = qobject_cast<const InputField*>(widget->parentWidget()))
                {
                    readOnly = inputTextField->isReadOnly();
                    valid = inputTextField->lastValidationResult() != QValidator::Invalid;
                }
                else if (auto lineEdit = qobject_cast<const QLineEdit*>(widget))
                {
                    readOnly = lineEdit->isReadOnly();
                }
                else if (auto spinBox = qobject_cast<const QAbstractSpinBox*>(widget))
                {
                    readOnly = spinBox->isReadOnly();
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

                    /* Obtain hover information.
                     * An item has enabled state here if whole widget is enabled
                     * and the item has Qt::ItemIsEnabled flag. */
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
        * All other views draw in PE_PanelItemViewItem:
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

                /* Obtain hover information: */
                auto hoverBrush = option->palette.midlight();
                bool hoverTransparent = hoverBrush.color().alpha() == 0;
                bool hasHover = item->state.testFlag(State_MouseOver) && !hoverTransparent;
                bool skipHoverDrawing = selectionOpaque;

                if (widget && !hoverTransparent)
                {
                    if (!widget->isEnabled() || widget->property(Properties::kSuppressHoverPropery).toBool())
                    {
                        /* Itemviews with kSuppressHoverProperty should never draw hover. */
                        hasHover = false;
                    }
                    else
                    {
                        /* Treeviews can handle hover in a special way. */
                        if (auto treeView = qobject_cast<const QTreeView*>(widget))
                        {
                            /* For items with Qt::ItemIsEnabled flag
                             * treeviews draws hover in PE_PanelItemViewRow. */
                            bool hoverDrawn = item->index.flags().testFlag(Qt::ItemIsEnabled);

                            /* For items without Qt::ItemIsEnabled flag we should
                             * draw hover only if selection behavior is SelectRows. */
                            skipHoverDrawing = hoverDrawn
                                || treeView->selectionBehavior() != QAbstractItemView::SelectRows;
                        }

                        /* Obtain Nx hovered row information: */
                        QVariant value = widget->property(Properties::kHoveredRowProperty);
                        if (value.isValid())
                            hasHover = value.toInt() == item->index.row();
                    }
                }

                /* Draw model-enforced background if needed: */
                QVariant background = item->index.data(Qt::BackgroundRole);
                if (background.isValid() && background.canConvert<QBrush>())
                    painter->fillRect(item->rect, background.value<QBrush>());

                /* Draw hover marker if needed: */
                if (hasHover && !skipHoverDrawing)
                    painter->fillRect(item->rect, hoverBrush);

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

        case PE_PanelTipLabel:
        {
            QnScopedPainterPenRollback penRollback(painter, QPen(option->palette.toolTipText(), 0));
            QnScopedPainterBrushRollback brushRollback(painter, option->palette.toolTipBase());
            QnScopedPainterAntialiasingRollback aaRollback(painter, true);

            static const qreal kToolTipRoundingRadius = 2.5;
            painter->drawRoundedRect(eroded(QRectF(option->rect), 0.5),
                kToolTipRoundingRadius, kToolTipRoundingRadius);

            return;
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

            QnScopedPainterPenRollback penRollback(painter);
            QnScopedPainterAntialiasingRollback aaRollback(painter, false);

            if (shape == TabShape::Default)
            {
                painter->fillRect(rect, option->palette.color(QPalette::Mid));

                painter->setPen(option->palette.color(QPalette::Window));
                painter->drawLine(rect.topLeft(), rect.topRight());

                painter->setPen(option->palette.color(QPalette::Base));
                painter->drawLine(rect.bottomLeft(), rect.bottomRight());
            }
            else
            {
                painter->setPen(option->palette.color(QPalette::Mid));
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

        case PE_IndicatorBranch:
        {
            if (!option->state.testFlag(State_Children))
                return;

            /* Do not draw branch marks in dropdowns: */
            if (isWidgetOwnedBy<QComboBox>(widget))
                return;

            auto icon = option->state.testFlag(State_Open)
                ? qnSkin->icon("tree/branch_open.png")
                : qnSkin->icon("tree/branch_closed.png");

            icon.paint(painter, option->rect);
            return;
        }

        case PE_IndicatorViewItemCheck:
        {
            QStyleOptionViewItem adjustedOption;
            bool exclusive = widget && widget->property(Properties::kItemViewRadioButtons).toBool();

            auto drawFunction =
                [exclusive, painter, widget, d](const QStyleOption* option)
                {
                    if (exclusive)
                        d->drawRadioButton(painter, option, widget);
                    else
                        d->drawCheckBox(painter, option, widget);
                };

            if (viewItemHoverAdjusted(widget, option, adjustedOption))
                drawFunction(&adjustedOption);
            else
                drawFunction(option);

            return;
        }

        case PE_IndicatorItemViewItemDrop:
        {
            if (!option->rect.isValid())
                return;

            QnScopedPainterPenRollback penRollback(painter, option->palette.color(QPalette::Link));
            painter->drawRect(option->rect.adjusted(0, 0, -1, -1));
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

            /* Main window tabs draw icons: */
            if (const QTabBar* tabBar = qobject_cast<const QTabBar*>(widget->parent()))
            {
                if (tabShape(tabBar) == TabShape::Rectangular)
                {
                    QIcon icon = qnSkin->icon(selected
                        ? lit("tab_bar/tab_close_current.png")
                        : lit("tab_bar/tab_close.png"));

                    QIcon::Mode mode = buttonIconMode(*option);
                    icon.paint(painter, option->rect, Qt::AlignCenter, mode);
                    return;
                }
            }

            /* Other tabs normally don't have close buttons, but if
             * they ever do, fall back to vector drawing of the cross: */

            QColor color = option->palette.color(
                    selected ? QPalette::Text : QPalette::Light);

            if (option->state.testFlag(State_Sunken))
            {
                QnPaletteColor background = findColor(
                    option->palette.color(selected ? QPalette::Midlight : QPalette::Dark)).darker(1);
                painter->fillRect(option->rect, background);
            }
            else if (option->state.testFlag(State_MouseOver))
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
                        /* Main window tabs draw icons: */
                        case TabShape::Rectangular:
                        {
                            QIcon icon;
                            switch (element)
                            {
                                case PE_IndicatorArrowLeft:
                                    icon = qnSkin->icon(lit("tab_bar/tab_prev.png"));
                                    break;
                                case PE_IndicatorArrowRight:
                                    icon = qnSkin->icon(lit("tab_bar/tab_next.png"));
                                    break;
                                default:
                                    break;
                            }

                            if (icon.isNull())
                            {
                                /* Fall back to vector drawing: */
                                width = 1.3;
                                break;
                            }

                            QIcon::Mode mode = buttonIconMode(*option);
                            icon.paint(painter, option->rect.adjusted(1, 0, 0, 0), Qt::AlignCenter, mode);
                            return;
                        }

                        /* Other tabs fall back to vector drawing of the arrows: */
                        case TabShape::Default:
                        case TabShape::Compact:
                            size = 14;
                            color = findColor(option->palette.light().color()).darker(2);
                            width = 1.5;
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
            break;
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
                const bool enabled = slider->state.testFlag(State_Enabled);
                const bool hovered = enabled && slider->state.testFlag(State_MouseOver);

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

                    SliderFeatures features = static_cast<SliderFeatures>(option->styleObject
                        ? option->styleObject->property(Properties::kSliderFeatures).toInt()
                        : 0);

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

                    if (enabled && option->activeSubControls.testFlag(SC_SliderHandle))
                        borderColor = mainLight.lighter(4);
                    else if (hovered)
                        borderColor = mainLight.lighter(2);

                    if (option->state.testFlag(State_Sunken))
                        fillColor = mainDark.lighter(3);

                    fillColor.setAlphaF(1.0);

                    painter->setPen(QPen(borderColor, 2));
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
                    QRect subtractRect = labelRect.united(subControlRect(CC_GroupBox, groupBox, SC_GroupBoxCheckBox, widget));

                    if (!panel && subtractRect.isValid())
                    {
                        painter->setClipRegion(QRegion(option->rect).subtracted(
                            subtractRect.adjusted(-Metrics::kStandardPadding, 0, Metrics::kStandardPadding, 0)));
                    }

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

                            rect.setLeft(rect.left() + QFontMetrics(font).size(kTextFlags, text).width() + Metrics::kStandardPadding);
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
                        const int kTextFlags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextHideMnemonic;
                        drawItemText(painter, labelRect, kTextFlags, groupBox->palette,
                            groupBox->state.testFlag(QStyle::State_Enabled),
                            groupBox->text, QPalette::WindowText);
                    }
                }

                if (groupBox->subControls.testFlag(SC_GroupBoxCheckBox))
                {
                    QStyleOption opt = *option;
                    opt.rect = subControlRect(CC_GroupBox, groupBox, SC_GroupBoxCheckBox, widget);

                    if (panel)
                    {
                        drawSwitch(painter, &opt, widget);
                    }
                    else
                    {
                        Q_D(const QnNxStyle);
                        opt.palette.setBrush(QPalette::Text, opt.palette.light());
                        d->drawCheckBox(painter, &opt, widget);
                    }
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
                    const QRect buttonRect = subControlRect(control, spinBox, subControl, widget);
                    const QnPaletteColor mainColor = findColor(spinBox->palette.color(QPalette::Base));

                    bool up = (subControl == SC_SpinBoxUp);
                    bool enabled = spinBox->state.testFlag(QStyle::State_Enabled) && spinBox->stepEnabled.testFlag(up ? QSpinBox::StepUpEnabled : QSpinBox::StepDownEnabled);

                    if (enabled && spinBox->activeSubControls.testFlag(subControl))
                    {
                        painter->fillRect(buttonRect, spinBox->state.testFlag(State_Sunken)
                            ? mainColor
                            : mainColor.lighter(1));
                    }

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

    if (element == CE_CheckBox && isSwitchButtonCheckbox(widget))
        element = CE_PushButton;

    switch (element)
    {
        case CE_ShapedFrame:
        {
            if (auto frame = qstyleoption_cast<const QStyleOptionFrame*>(option))
            {
                /* Special frame for multiline text edits: */
                if (qobject_cast<const QPlainTextEdit*>(widget) || qobject_cast<const QTextEdit*>(widget))
                {
                    /* If NoFrame shape is set, draw no panel: */
                    if (static_cast<const QFrame*>(widget)->frameShape() == QFrame::NoFrame)
                        return;

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
                static constexpr int kComboBoxMarginAdjustment = 7; //< to align with other controls
                QStyleOptionComboBox opt(*comboBox);
                if (!isIconListCombo(qobject_cast<const QComboBox*>(widget)))
                    opt.rect.setLeft(opt.rect.left() + kComboBoxMarginAdjustment);
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

        case CE_ToolButtonLabel:
        {
            if (auto button = qstyleoption_cast<const QStyleOptionToolButton*>(option))
            {
                if (button->state.testFlag(State_Enabled) && button->state.testFlag(State_Sunken))
                {
                    QStyleOptionToolButton optionCopy(*button);
                    optionCopy.state &= ~State_MouseOver;
                    optionCopy.icon = QnSkin::maximumSizePixmap(button->icon, QnIcon::Pressed,
                        button->state.testFlag(State_On) ? QIcon::On : QIcon::Off, false);
                    d->drawToolButton(painter, &optionCopy, widget);
                }
                else
                {
                    d->drawToolButton(painter, button, widget);
                }
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
                            QnPaletteColor mainColor = findColor(option->palette.color(QPalette::Mid)).lighter(1);
                            painter->fillRect(tab->rect.adjusted(0, 1, 0, -1), mainColor);

                            QnScopedPainterPenRollback penRollback(painter, mainColor.darker(3).color());
                            painter->drawLine(tab->rect.topLeft(), tab->rect.topRight());
                        }
                        break;
                    }

                    case TabShape::Rectangular:
                    {
                        QnPaletteColor mainColor = findColor(
                            option->palette.color(QPalette::Base)).lighter(1);

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

                int iconWithPadding = 0;
                if (!tab->icon.isNull())
                {
                    QSize iconSize = QnSkin::maximumSize(tab->icon);
                    iconWithPadding = iconSize.width() + Metrics::kStandardPadding;

                    QRect iconRect = textRect;
                    iconRect.setWidth(iconWithPadding);
                    iconRect = aligned(iconSize, iconRect);
                    tab->icon.paint(painter, iconRect);
                }

                if (shape == TabShape::Rectangular)
                {
                    textFlags |= Qt::AlignLeft | Qt::AlignVCenter;

                    if (tab->state.testFlag(QStyle::State_Selected))
                        color = tab->palette.text().color();
                    else
                        color = tab->palette.light().color();

                    int hspace = pixelMetric(PM_TabBarTabHSpace, option, widget);
                    textRect.adjust(qMax(iconWithPadding, hspace), 0, -hspace, 0);
                    focusRect.adjust(0, 2, 0, -2);
                }
                else
                {
                    textFlags |= Qt::AlignCenter;
                    textRect.setLeft(textRect.left() + iconWithPadding);

                    QnPaletteColor mainColor = findColor(tab->palette.light().color()).darker(2);
                    color = mainColor;

                    QFontMetrics fm(painter->font());
                    QRect rect = fm.boundingRect(textRect, textFlags, tab->text);

                    if (shape == TabShape::Compact)
                    {
                        rect.setBottom(textRect.bottom());
                        rect.setTop(rect.bottom());
                        focusRect.setLeft(rect.left() - kCompactTabFocusMargin);
                        focusRect.setRight(rect.right() + kCompactTabFocusMargin);
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
                                option->palette.color(QPalette::Mid)).lighter(3));
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
                bool hovered = header->state.testFlag(State_MouseOver);
                if (auto headerWidget = qobject_cast<const QHeaderView*>(widget))
                {
                    /* In addition to Qt's requirement of sectionsClickable()
                     * we demand isSortIndicatorShown() to be also true to enable hover: */
                    if (!headerWidget->isSortIndicatorShown())
                        hovered = false;
                }

                if (hovered)
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
                bool asDropdown = widget && widget->property(Properties::kMenuAsDropdown).toBool();
                if (menuItem->menuItemType == QStyleOptionMenuItem::Separator)
                {
                    QnScopedPainterPenRollback penRollback(painter, menuItem->palette.color(QPalette::Midlight));
                    int y = menuItem->rect.top() + menuItem->rect.height() / 2;
                    int padding = asDropdown ? Metrics::kStandardPadding: Metrics::kMenuItemHPadding;
                    painter->drawLine(padding, y,
                                      menuItem->rect.right() - padding, y);
                    return;
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

                int xMargin = asDropdown
                    ? Metrics::kStandardPadding
                    : Metrics::kMenuItemTextLeftPadding;

                int y = menuItem->rect.y();

                int textFlags = Qt::AlignVCenter | Qt::TextHideMnemonic | Qt::TextDontClip | Qt::TextSingleLine;

                QRect textRect(menuItem->rect.left() + xMargin,
                               y + Metrics::kMenuItemVPadding,
                               menuItem->rect.width() - xMargin - Metrics::kMenuItemHPadding,
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
                    NX_ASSERT(!asDropdown, Q_FUNC_INFO, "Not supported");
                    drawMenuCheckMark(
                            painter,
                            QRect(Metrics::kMenuItemHPadding, menuItem->rect.y(), 16, menuItem->rect.height()),
                            textColor);
                }

                if (menuItem->menuItemType == QStyleOptionMenuItem::SubMenu)
                {
                    NX_ASSERT(!asDropdown, Q_FUNC_INFO, "Not supported");
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
                        pos += rect.left();
                        if (progressBar->invertedAppearance)
                            rect.setLeft(pos);
                        else
                            rect.setRight(pos);
                    }
                    else
                    {
                        pos += rect.top();
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

        case CE_PushButtonBevel:
        {
            if (QnNxStylePrivate::isTextButton(option))
                return;

            break;
        }

        case CE_PushButtonLabel:
        {
            if (auto buttonOption = static_cast<const QStyleOptionButton*>(option))
            {
                /* Support for custom foreground role for buttons: */
                QPalette::ColorRole foregroundRole = widget
                    ? widget->foregroundRole()
                    : QPalette::ButtonText;
                const bool isDefaultForegroundRole = (foregroundRole == QPalette::ButtonText);

                /* Draw text button: */
                if (QnNxStylePrivate::isTextButton(option))
                {
                    /* Foreground role override: */
                    if (isDefaultForegroundRole)
                        foregroundRole = QPalette::WindowText;

                    d->drawTextButton(painter, buttonOption, foregroundRole, widget);
                    return;
                }

                if (isDefaultForegroundRole && isWarningStyle(widget))
                    foregroundRole = QPalette::BrightText;
                else if (isDefaultForegroundRole && isAccented(widget))
                    foregroundRole = QPalette::HighlightedText;

                int margin = pixelMetric(PM_ButtonMargin, option, widget);
                QRect textRect = option->rect.adjusted(margin, 0, -margin, 0);

                Qt::Alignment textHorizontalAlignment = Qt::AlignHCenter;

                if (widget && widget->property(Properties::kPushButtonMargin).canConvert<int>())
                    textHorizontalAlignment = Qt::AlignLeft;

                /* Draw icon left-aligned: */
                if (!buttonOption->icon.isNull())
                {
                    QIcon::Mode mode = buttonIconMode(*option);

                    QIcon::State state = buttonOption->state.testFlag(State_On)
                        ? QIcon::On : QIcon::Off;

                    QRect iconRect = buttonOption->rect;
                    iconRect.setWidth(buttonOption->iconSize.width() + margin);

                    if (buttonOption->direction == Qt::RightToLeft)
                    {
                        iconRect.moveRight(buttonOption->rect.right());
                        textRect.setRight(iconRect.left() - 1);
                    }
                    else
                    {
                        textRect.setLeft(option->rect.left() + iconRect.width());
                    }

                    iconRect = alignedRect(option->direction, Qt::AlignCenter,
                        buttonOption->iconSize, iconRect);

                    buttonOption->icon.paint(painter, iconRect, Qt::AlignCenter, mode, state);
                    textHorizontalAlignment = Qt::AlignLeft;
                }

                /* Draw switch right-aligned: */
                if (QnNxStylePrivate::isCheckableButton(option))
                {
                    QStyleOptionButton newOpt(*buttonOption);
                    newOpt.rect.setWidth(Metrics::kButtonSwitchSize.width());
                    newOpt.rect.setBottom(newOpt.rect.bottom() - 1); // shadow compensation

                    if (buttonOption->direction == Qt::RightToLeft)
                    {
                        newOpt.rect.moveLeft(option->rect.left() + Metrics::kSwitchMargin);
                        textRect.setLeft(newOpt.rect.right() + Metrics::kStandardPadding + 1);
                    }
                    else
                    {
                        newOpt.rect.moveRight(option->rect.right() - Metrics::kSwitchMargin);
                        textRect.setRight(newOpt.rect.left() - Metrics::kStandardPadding - 1);
                    }

                    drawSwitch(painter, &newOpt, widget);
                    textHorizontalAlignment = Qt::AlignLeft;
                }

                /* Subtract menu indicator area: */
                if (buttonOption->features.testFlag(QStyleOptionButton::HasMenu))
                {
                    int indicatorSize = proxy()->pixelMetric(PM_MenuButtonIndicator, option, widget);
                    if (buttonOption->direction == Qt::RightToLeft)
                        textRect.setLeft(textRect.left() + indicatorSize);
                    else
                        textRect.setRight(textRect.right() - indicatorSize);
                }

                /* Draw text: */
                if (!buttonOption->text.isEmpty())
                {
                    const int textFlags = textHorizontalAlignment
                        | Qt::AlignVCenter
                        | Qt::TextSingleLine
                        | Qt::TextHideMnemonic;

                    /* Measurements don't support Qt::TextHideMnemonic, must use Qt::TextShowMnemonic: */
                    const auto fixedText = nx::utils::removeMnemonics(buttonOption->text);
                    const QString text = buttonOption->fontMetrics.elidedText(fixedText,
                        Qt::ElideRight, textRect.width(), Qt::TextShowMnemonic);

                    proxy()->drawItemText(painter, textRect, textFlags, buttonOption->palette,
                        buttonOption->state.testFlag(State_Enabled), text, foregroundRole);
                }

                return;
            }

            break;
        }

        case CE_Splitter:
        {
            QStyleOptionFrame frameOption;
            frameOption.QStyleOption::operator = (*option);
            frameOption.frameShape = option->state.testFlag(State_Horizontal) ? QFrame::VLine : QFrame::HLine;
            frameOption.lineWidth = 1;
            frameOption.state |= State_Sunken;
            proxy()->drawControl(CE_ShapedFrame, &frameOption, painter, widget);
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
    switch (control)
    {
        case CC_Slider:
        {
            if (auto slider = qstyleoption_cast<const QStyleOptionSlider*>(option))
            {
                QRect rect = base_type::subControlRect(CC_Slider, option, subControl, widget);
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
                        const int kGrooveWidth = 4;

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

                return rect;
            }
            break;
        }

        case CC_ComboBox:
        {
            if (auto comboBox = qstyleoption_cast<const QStyleOptionComboBox*>(option))
            {
                QRect rect;
                switch (subControl)
                {
                    case SC_ComboBoxArrow:
                    {
                        rect = QRect(comboBox->rect.right() - Metrics::kButtonHeight,
                                     comboBox->rect.top(),
                                     Metrics::kButtonHeight,
                                     comboBox->rect.height());
                        rect.adjust(1, 1, 0, -1);
                        break;
                    }

                    case SC_ComboBoxEditField:
                    {
                        int frameWidth = pixelMetric(PM_ComboBoxFrameWidth, option, widget);
                        rect = comboBox->rect;
                        rect.setRight(rect.right() - Metrics::kButtonHeight);
                        rect.adjust(frameWidth, frameWidth, 0, -frameWidth);
                        break;
                    }

                    case SC_ComboBoxListBoxPopup:
                    {
                        rect = base_type::subControlRect(CC_ComboBox, option, subControl, widget);
                        if (widget)
                        {
                            const int width = widget->property(Properties::kComboBoxPopupWidth).toInt();
                            if (width != 0)
                                rect.setWidth(width);
                        }

                        break;
                    }

                    default:
                        rect = base_type::subControlRect(CC_ComboBox, option, subControl, widget);
                        break;
                }

                return rect;
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
                const bool panel = groupBox->features.testFlag(QStyleOptionFrame::Flat);
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

                        /* Measurements don't support Qt::TextHideMnemonic,
                         * so if we draw with Qt::TextHideMnemonic we
                         * must measure with Qt::TextShowMnemonic: */
                        const int kTextFlags = Qt::AlignLeft | Qt::TextShowMnemonic;
                        int left = option->rect.left();

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

                            int textWidth = QFontMetrics(font).size(kTextFlags, text).width();

                            if (!detailText.isEmpty())
                            {
                                int detailWidth = groupBox->fontMetrics.size(kTextFlags, detailText).width();
                                textWidth += Metrics::kStandardPadding + detailWidth;
                            }

                            if (groupBox->subControls.testFlag(SC_GroupBoxCheckBox))
                            {
                                left = subControlRect(CC_GroupBox, option, SC_GroupBoxCheckBox, widget).right()
                                    + 1 + Metrics::kSwitchMargin;
                            }

                            return QRect(left, option->rect.top(), textWidth, Metrics::kPanelHeaderHeight);
                        }

                        if (groupBox->subControls.testFlag(SC_GroupBoxCheckBox))
                        {
                            left = subControlRect(CC_GroupBox, option, SC_GroupBoxCheckBox, widget).right()
                                + 1 + proxy()->pixelMetric(PM_CheckBoxLabelSpacing, option, widget);;
                        }
                        else
                        {
                            left += Metrics::kGroupBoxContentMargins.left();
                        }

                        return QRect(
                            left,
                            option->rect.top(),
                            groupBox->fontMetrics.size(kTextFlags, groupBox->text).width(),
                            Metrics::kGroupBoxTopMargin * 2);
                    }

                    case SC_GroupBoxCheckBox:
                    {
                        if (!groupBox->subControls.testFlag(SC_GroupBoxCheckBox))
                            return QRect();

                        QRect headerRect(option->rect);
                        if (panel)
                        {
                            headerRect.setHeight(Metrics::kPanelHeaderHeight - 1);
                            return alignedRect(Qt::LeftToRight, Qt::AlignLeft | Qt::AlignVCenter,
                                Metrics::kStandaloneSwitchSize + kSwitchFocusFrameMargins + QSize(0, 1), headerRect);
                        }

                        headerRect.setLeft(headerRect.left() + Metrics::kGroupBoxContentMargins.left());
                        headerRect.setHeight(Metrics::kGroupBoxTopMargin * 2);

                        return alignedRect(Qt::LeftToRight, Qt::AlignLeft | Qt::AlignVCenter,
                            QSize(Metrics::kCheckIndicatorSize, Metrics::kCheckIndicatorSize), headerRect);
                    }

                    case SC_GroupBoxContents:
                    {
                        const auto contentsMargins = groupBoxContentsMargins(panel, widget);
                        return option->rect.marginsRemoved(contentsMargins);
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
                QRect rect = scrollBar->rect;
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
                        break;

                    case SC_ScrollBarSlider:
                    case SC_ScrollBarAddPage:
                    case SC_ScrollBarSubPage:
                    {
                        qreal range = scrollBar->maximum - scrollBar->minimum;
                        int maxLength = scrollBar->orientation == Qt::Vertical
                            ? rect.height()
                            : rect.width();

                        int minLength = proxy()->pixelMetric(PM_ScrollBarSliderMin, scrollBar, widget);

                        int sliderLength = static_cast<int>(
                            (static_cast<qreal>(maxLength) * scrollBar->pageStep)
                                / (range + scrollBar->pageStep) + 0.5);

                        sliderLength = qBound(minLength, sliderLength, maxLength);

                        int pos = sliderPositionFromValue(
                            scrollBar->minimum,
                            scrollBar->maximum,
                            scrollBar->sliderPosition,
                            maxLength - sliderLength,
                            scrollBar->upsideDown);

                        switch (subControl)
                        {
                            case SC_ScrollBarSlider:
                                if (scrollBar->orientation == Qt::Vertical)
                                    rect = QRect(rect.left(), rect.top() + pos, rect.width(), sliderLength);
                                else
                                    rect = QRect(rect.left() + pos, rect.top(), sliderLength, rect.height());
                                break;

                            case SC_ScrollBarAddPage:
                                if (scrollBar->orientation == Qt::Vertical)
                                    rect = QRect(QPoint(rect.left(), rect.top() + pos + sliderLength), rect.bottomRight());
                                else
                                    rect = QRect(QPoint(rect.left() + pos + sliderLength, rect.top()), rect.bottomRight());
                                break;

                            case SC_ScrollBarSubPage:
                                if (scrollBar->orientation == Qt::Vertical)
                                    rect = QRect(rect.left(), rect.top(), rect.width(), pos);
                                else
                                    rect = QRect(rect.left(), rect.top(), pos, rect.height());
                                break;

                            default:
                                break;
                        }

                        break;
                    }

                    case SC_ScrollBarFirst:
                    case SC_ScrollBarLast:
                    default:
                        return QRect();
                }

                return visualRect(scrollBar->direction, scrollBar->rect, rect);
            }

            break;
        }

        default:
            break;
    }

    return base_type::subControlRect(control, option, subControl, widget);
}

QRect QnNxStyle::subElementRect(
        QStyle::SubElement subElement,
        const QStyleOption *option,
        const QWidget *widget) const
{
    switch (subElement)
    {
        case SE_CheckBoxClickRect:
        {
            if (isSwitchButtonCheckbox(widget))
                return option->rect;

            break;
        }

        case SE_LineEditContents:
        {
            const auto standardRect = base_type::subElementRect(subElement, option, widget);

            if (isItemViewEdit(widget))
                return standardRect;

            static const int kLineEditIndent = 6;
            static const int kCalendarYearEditIndent = 4;

            if (isWidgetOwnedBy<QCalendarWidget>(widget))
                return standardRect.adjusted(kCalendarYearEditIndent, 0, 0, 0);

            return standardRect.adjusted(kLineEditIndent, 0, 0, 0);
        }

        case SE_PushButtonLayoutItem:
        {
            if (auto buttonBox = qobject_cast<const QDialogButtonBox *>(widget))
            {
                if (qobject_cast<const QnDialog*>(buttonBox->parentWidget()))
                {
                    int margin = proxy()->pixelMetric(PM_DefaultTopLevelMargin);
                    return QnGeometry::dilated(option->rect, margin);
                }
            }
            break;
        }

        case SE_PushButtonFocusRect:
        {
            return QnNxStylePrivate::isTextButton(option)
                ? option->rect
                : QnGeometry::eroded(option->rect, 1);
        }

        case SE_ProgressBarGroove:
        {
            if (auto progressBar = qstyleoption_cast<const QStyleOptionProgressBar *>(option))
            {
                const bool hasText = progressBar->textVisible;
                const int kProgressBarWidth = 4;
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
                if (!progressBar->textVisible)
                    break;

                if (progressBar->orientation == Qt::Horizontal)
                    return progressBar->rect.adjusted(0, 0, 0, -8);
                else
                    return progressBar->rect.adjusted(8, 0, 0, 0);
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
                    rect.moveLeft(rect.left() - hspace + kCompactTabFocusMargin);
                else
                    rect.moveLeft(rect.left() + hspace);

                int indent = tabWidget->property(Properties::kTabBarIndent).toInt();
                rect.moveLeft(rect.left() + indent);

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
                return aligned(size, tabBar->rect, Qt::AlignRight | Qt::AlignVCenter);
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
                QnIndents indents = itemViewItemIndents(item);

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

                rect = option->fontMetrics.boundingRect(
                    rect,
                    Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic,
                    header->text);

                if (!header->icon.isNull())
                {
                    /* To match Qt standard drawing: */
                    int iconSize = proxy()->pixelMetric(QStyle::PM_SmallIconSize, header, widget);
                    QSize size = header->icon.actualSize(widget->windowHandle(), QSize(iconSize, iconSize),
                        header->state.testFlag(State_Enabled) ? QIcon::Normal : QIcon::Disabled);

                    rect.setWidth(rect.width() + size.width() + kQtHeaderIconMargin);
                    rect.setHeight(qMax(rect.height(), size.height()));
                }

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
    const QStyleOption* option,
    const QSize& size,
    const QWidget* widget) const
{
    if (type == CT_CheckBox && isSwitchButtonCheckbox(widget))
        type = CT_PushButton;

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
            const auto button = qstyleoption_cast<const QStyleOptionButton*>(option);
            const bool hasIcon = button && !button->icon.isNull();
            const bool hasText = button && !button->text.isEmpty();
            const bool textButton = QnNxStylePrivate::isTextButton(option);

            QSize result(size.width(), qMax(size.height(), Metrics::kButtonHeight));
            if (hasText)
            {
                result.rwidth() += pixelMetric(PM_ButtonMargin, option, widget) * 2;
                if (hasIcon)
                    result.rwidth() -= 4; // Compensate for QPushButton::sizeHint magic
            }

            if (textButton)
                result.rwidth() += (hasIcon ? Metrics::kTextButtonIconMargin : 0);
            else if (hasText)
                result.rwidth() = qMax(result.rwidth(), Metrics::kMinimumButtonWidth);
            else // Make button at least square
                result.rwidth() = qMax(result.rwidth(), result.rheight());

            if (QnNxStylePrivate::isCheckableButton(option))
            {
                const QSize switchSize = textButton
                    ? Metrics::kStandaloneSwitchSize
                    : Metrics::kButtonSwitchSize;

                result.rwidth() += switchSize.width() + Metrics::kStandardPadding * 2
                    - Metrics::kSwitchMargin;

                result.rheight() = qMax(result.rheight(), switchSize.height());
            }

            return result;
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
                    constexpr int kSafetyMargin = 8; //< as QDateTimeEdit calcs size not by the longest possible string
                    int hMargin = pixelMetric(PM_ButtonMargin, option, widget);
                    int height = qMax(size.height(), Metrics::kButtonHeight);
                    int buttonWidth = height;
                    int width = qMax(Metrics::kMinimumButtonWidth,
                        size.width() + hMargin + kSafetyMargin + buttonWidth);
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
            int width = size.width() + (hasArrow ? Metrics::kButtonHeight : hMargin);

            if (!isIconListCombo(qobject_cast<const QComboBox*>(widget)))
                width = qMax(Metrics::kMinimumButtonWidth, width + hMargin);

            return QSize(width, height);
        }

        case CT_GroupBox:
        {
            if (auto groupBox = qstyleoption_cast<const QStyleOptionGroupBox*>(option))
            {
                const bool panel = groupBox->features.testFlag(QStyleOptionFrame::Flat);
                const auto contentsMargins = groupBoxContentsMargins(panel, widget);

                return size + QSize(
                    contentsMargins.left() + contentsMargins.right(),
                    contentsMargins.top()  + contentsMargins.bottom());
            }

            break;
        }

        case CT_TabBarTab:
        {
            if (qstyleoption_cast<const QStyleOptionTab*>(option))
            {
                TabShape shape = tabShape(widget);

                const int kRoundedSize = 36 + 1; /* shadow */
                const int kCompactSize = 32;

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
        {
            if (widget && widget->property(Properties::kMenuAsDropdown).toBool())
            {
                return QSize(
                    qMax(size.width() + 2 * Metrics::kStandardPadding, Metrics::kMinimumButtonWidth),
                    size.height() + 2 * Metrics::kMenuItemVPadding);
            }

            return QSize(size.width() + 24 + 2 * Metrics::kMenuItemHPadding,
                size.height() + 2 * Metrics::kMenuItemVPadding);
        }

        case CT_HeaderSection:
        {
            if (auto header = qstyleoption_cast<const QStyleOptionHeader *>(option))
            {
                QSize textSize = header->fontMetrics.size(Qt::TextShowMnemonic, header->text);

                int width = textSize.width();

                width += 2 * Metrics::kStandardPadding;

                if (header->sortIndicator != QStyleOptionHeader::None)
                {
                    width += pixelMetric(PM_HeaderMargin, header, widget);
                    width += pixelMetric(PM_HeaderMarkSize, header, widget);
                }

                if (!header->icon.isNull())
                {
                    int iconSize = proxy()->pixelMetric(QStyle::PM_SmallIconSize, header, widget);
                    width += iconSize + kQtHeaderIconMargin;
                }

                static constexpr int kUnusedHeight = 0;

                const int height = header->orientation == Qt::Horizontal
                    ? qMax(textSize.height(), Metrics::kHeaderSize)
                    : kUnusedHeight; //< vertical header height is calculated elsewhere

                return QSize(width, height);
            }
            break;
        }

        case CT_ItemViewItem:
        {
            if (const QStyleOptionViewItem* item = qstyleoption_cast<const QStyleOptionViewItem*>(option))
            {
                QnIndents indents = itemViewItemIndents(item);

                if (isCheckboxOnlyItem(*item))
                {
                    return QSize(indents.left() + indents.right() + Metrics::kCheckIndicatorSize,
                        Metrics::kCheckIndicatorSize);
                }

                QSize size = base_type::sizeFromContents(type, option, size, widget);

                int minHeight = qobject_cast<const QListView*>(item->widget)
                    ? Metrics::kListRowHeight
                    : Metrics::kViewRowHeight;

                size.setHeight(qMax(size.height(), minHeight));
                size.setWidth(size.width() + indents.left() + indents.right() -
                    (pixelMetric(PM_FocusFrameHMargin, option, widget) + 1) * 2);

                return size;
            }

            break;
        }

        case CT_ProgressBar:
        {
            if (auto progressBar = qstyleoption_cast<const QStyleOptionProgressBar*>(option))
            {
                if (!progressBar->textVisible)
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
    const QStyleOption* option,
    const QWidget* widget) const
{
    switch (metric)
    {
        case PM_ButtonMargin:
        {
            if (qobject_cast<const QAbstractItemView*>(widget))
                return 0;

            if (QnNxStylePrivate::isCheckableButton(option))
                return Metrics::kSwitchMargin * 2;

            if (QnNxStylePrivate::isTextButton(option))
                return 0;

            if (auto button = qstyleoption_cast<const QStyleOptionButton*>(option))
            {
                const bool hasText = !button->text.isEmpty();
                const bool hasIcon = !button->icon.isNull();
                if (hasIcon)
                {
                    return hasText
                        ? Metrics::kPushButtonIconMargin * 2
                        : Metrics::kPushButtonIconOnlyMargin * 2;
                }
            }

            if (widget)
            {
                bool ok;
                int margin = widget->property(Properties::kPushButtonMargin).toInt(&ok);
                if (ok && margin >= 0)
                    return margin;
            }

            return Metrics::kStandardPadding * 2;
        }

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
            return 1;

        case PM_HeaderDefaultSectionSizeVertical:
            return Metrics::kViewRowHeight;
        case PM_HeaderMarkSize:
            return Metrics::kSortIndicatorSize;
        case PM_HeaderMargin:
            return 6;

        case PM_LayoutBottomMargin:
        case PM_LayoutLeftMargin:
        case PM_LayoutRightMargin:
        case PM_LayoutTopMargin:
        {
            if (!widget)
                return proxy()->pixelMetric(PM_DefaultChildMargin);

            if (qobject_cast<const QnDialog*>(widget) ||
                qobject_cast<const QDialogButtonBox*>(widget)) // button box has outer margins
            {
                return 0;
            }

            if (qobject_cast<const QnAbstractPreferencesWidget*>(widget) ||
                qobject_cast<const QnDialog*>(widget->parentWidget()) ||
                widget->isWindow() /*but not dialog*/)
            {
                return proxy()->pixelMetric(PM_DefaultTopLevelMargin);
            }

            return proxy()->pixelMetric(PM_DefaultChildMargin);
        }

        case PM_LayoutHorizontalSpacing:
            return Metrics::kDefaultLayoutSpacing.width();
        case PM_LayoutVerticalSpacing:
            return (qobject_cast<const QnDialog*>(widget)) ? 0 : Metrics::kDefaultLayoutSpacing.height();

        case PM_MenuButtonIndicator:
            return 20 + Metrics::kMenuButtonIndicatorMargin;

        case PM_MenuVMargin:
            return 2;
        case PM_SubMenuOverlap:
            return 1;

        case PM_SliderControlThickness:
            return 16;
        case PM_SliderThickness:
            return 20;
        case PM_SliderLength:
        {
            if (option && option->styleObject)
            {
                bool ok(false);
                int result = option->styleObject->property(Properties::kSliderLength).toInt(&ok);
                if (ok && result >= 0)
                    return result;
            }
            return 16;
        }

        case PM_ScrollView_ScrollBarOverlap:
            return qobject_cast<const QnScrollBarProxy*>(widget) ? 1 : 0;
        case PM_ScrollBarExtent:
            return qobject_cast<const QnScrollBarProxy*>(widget) ? 9 : 8;
        case PM_ScrollBarSliderMin:
            return 24;

        case PM_SplitterWidth:
            return 5;

        case PM_TabBarTabHSpace:
        case PM_TabBarTabVSpace:
            return tabShape(widget) == TabShape::Rectangular ? 8 : 20;

        case PM_TabBarScrollButtonWidth:
            return 24 + 1;

        case PM_TabCloseIndicatorWidth:
        case PM_TabCloseIndicatorHeight:
            return 24;

        case PM_ToolBarIconSize:
            return 32;

        case PM_ToolTipLabelFrameWidth:
            return 1;

        case PM_SmallIconSize:
            if (qobject_cast<const QMenu*>(widget))
                return 0; //< we don't show icons in menus
            /* FALL THROUGH */
        case PM_ListViewIconSize:
        case PM_TabBarIconSize:
        case PM_ButtonIconSize:
            return Metrics::kDefaultIconSize;

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
        case SH_ScrollBar_ContextMenu:
            return 0;
        default:
            break;
    }

    return base_type::styleHint(sh, option, widget, shret);
}

QIcon QnNxStyle::standardIcon(StandardPixmap iconId, const QStyleOption* option, const QWidget* widget) const
{
    switch (iconId)
    {
        case SP_LineEditClearButton:
            return qnSkin->icon("standard_icons/sp_line_edit_clear_button.png");
        case SP_ArrowLeft:
        case SP_ArrowBack:
            return qnSkin->icon("standard_icons/sp_arrow_back.png");
        case SP_ArrowRight:
        case SP_ArrowForward:
            return qnSkin->icon("standard_icons/sp_arrow_forward.png");
        case SP_FileDialogToParent:
            return qnSkin->icon("standard_icons/sp_file_dialog_to_parent.png");
        case SP_FileDialogListView:
            return qnSkin->icon("standard_icons/sp_file_dialog_list_view.png");
        case SP_FileDialogDetailedView:
            return qnSkin->icon("standard_icons/sp_file_dialog_detailed_view.png");
        case SP_FileDialogNewFolder:
            return qnSkin->icon("standard_icons/sp_file_dialog_new_folder.png");
        case SP_MessageBoxInformation:
            return qnSkin->icon("standard_icons/sp_message_box_information.png");
        case SP_MessageBoxQuestion:
            return qnSkin->icon("standard_icons/sp_message_box_question.png");
        case SP_MessageBoxWarning:
            return qnSkin->icon("standard_icons/sp_message_box_warning.png");
        case SP_MessageBoxCritical:
            return qnSkin->icon("standard_icons/sp_message_box_critical.png");

        default:
            auto baseIcon = base_type::standardIcon(iconId, option, widget);
            /* Let leave it here for now to make sure we will not miss important icons. */
            if (!baseIcon.isNull())
                qDebug() << "Requested NOT_NULL standard icon!!!!" << iconId;
            return baseIcon;
    }
}

void QnNxStyle::polish(QWidget *widget)
{
    base_type::polish(widget);

    /* #QTBUG 18838 */
    /* Workaround for incorrectly updated hover state inside QGraphicsProxyWidget. */
    Q_D(QnNxStyle);
    if (d->graphicsProxyWidget(widget))
        widget->installEventFilter(this);

    if (qobject_cast<QAbstractSpinBox*>(widget) ||
        qobject_cast<QAbstractButton*>(widget) ||
        qobject_cast<QAbstractSlider*>(widget) ||
        qobject_cast<QGroupBox*>(widget))
    {
        widget->setAttribute(Qt::WA_Hover);

        /*
        * Fix for Qt 5.6 bug: QDateTimeEdit doesn't calculate hovered subcontrol rect
        *  which causes calendar dropdown button to not redraw properly
        */
        #if QT_VERSION < 0x050600 && QT_VERSION > 0x050602
        #error Check if this workaround is required in current Qt version
        #endif
        if (qobject_cast<QDateTimeEdit*>(widget))
            widget->installEventFilter(this);
    }

    if (auto calendar = qobject_cast<QCalendarWidget*>(widget))
    {
        if (!calendar->property(Properties::kDontPolishFontProperty).toBool())
        {
            QTextCharFormat header = calendar->headerTextFormat();
            QFont font = header.font();
            font.setWeight(QFont::Bold);
            font.setPixelSize(Metrics::kCalendarHeaderFontPixelSize);
            header.setFont(font);
            calendar->setHeaderTextFormat(header);

            /* To update text formats when palette changes: */
            calendar->installEventFilter(this);
        }
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
            font.setPixelSize(Metrics::kHeaderViewFontPixelSize);
            widget->setFont(font);
        }
        widget->setAttribute(Qt::WA_Hover);

        #if QT_VERSION < 0x050600 && QT_VERSION > 0x050602
        #error Check if this workaround is required in current Qt version
        #endif
        /* Fix for Qt 5.6 bug: QHeaderView doesn't resize stretch sections to minimum
         *  if quickly resized down. To overcome this problem we do it ourselves: */
        widget->installEventFilter(this);
    }

    if (auto lineEdit = qobject_cast<QLineEdit*>(widget))
    {
        if (!lineEdit->property(Properties::kDontPolishFontProperty).toBool()
            && !isItemViewEdit(lineEdit))
        {
            QFont font = lineEdit->font();

            if (isWidgetOwnedBy<QCalendarWidget>(lineEdit))
            {
                QPalette palette = lineEdit->palette();
                palette.setBrush(QPalette::Highlight, qApp->palette().highlight());
                palette.setBrush(QPalette::HighlightedText, qApp->palette().highlightedText());
                lineEdit->setPalette(palette);
                font.setBold(true);
            }
            else
            {
                font.setPixelSize(Metrics::kTextEditFontPixelSize);
            }

            lineEdit->setFont(font);
        }
    }

    if (qobject_cast<QPlainTextEdit*>(widget) || qobject_cast<QTextEdit*>(widget))
    {
        /* Adjust margins only for text edits with frame: */
        if (static_cast<const QFrame*>(widget)->frameShape() != QFrame::NoFrame)
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
            int v = 6 - kTextDocumentDefaultMargin;
            area->setViewportMargins(QMargins(h, v, h, v));
        }

        if (!widget->property(Properties::kDontPolishFontProperty).toBool())
        {
            QFont font = widget->font();
            font.setPixelSize(Metrics::kTextEditFontPixelSize);
            widget->setFont(font);
        }
    }

    /* Polish tooltips: */
    if ((widget->windowFlags() & Qt::ToolTip) == Qt::ToolTip
        && widget->contentsMargins().isNull()
        && qobject_cast<QLabel*>(widget))
    {
        QnTypedPropertyBackup<QMargins, QWidget>::backup(widget, &QWidget::contentsMargins,
            QN_SETTER(QWidget::setContentsMargins), kContentsMarginsBackupId);

        int qtTooltipMargin = 1 + proxy()->pixelMetric(PM_ToolTipLabelFrameWidth, 0, widget);

        int h = qMax(Metrics::kStandardPadding - qtTooltipMargin, 0);
        int v = qMax((Metrics::kToolTipHeight - widget->fontMetrics().height() + 1) / 2
            - qtTooltipMargin, 0);

        widget->setContentsMargins(h, v, h, v);

        /* To have rounded corners without using a window mask: */
        widget->setWindowFlags(widget->windowFlags() | Qt::FramelessWindowHint);
        widget->setAttribute(Qt::WA_TranslucentBackground);
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

    if (auto scrollBar = qobject_cast<QScrollBar*>(widget))
    {
        /* In certain containers we want scrollbars with transparent groove: */
        widget->setAttribute(Qt::WA_OpaquePaintEvent, false);
        widget->setAttribute(Qt::WA_Hover);

        /* Workaround to update scroll areas hover when they're scrolled: */
        auto updateScrollAreaHover =
            [d, guardedScrollBar = QPointer<QScrollBar>(scrollBar)]()
            {
                if (guardedScrollBar)
                    d->updateScrollAreaHover(guardedScrollBar);
            };

        connect(scrollBar, &QScrollBar::valueChanged,
            this, updateScrollAreaHover, Qt::QueuedConnection);
    }

    if (qobject_cast<QTabBar*>(widget))
    {
        if (!widget->property(Properties::kDontPolishFontProperty).toBool())
        {
            QFont font = widget->font();
            font.setPixelSize(Metrics::kTabBarFontPixelSize);
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
            effect->setXOffset(-4.0);
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
        else
        {
            /* Modify calendar item view: */
            if (auto calendar = isWidgetOwnedBy<QCalendarWidget>(widget))
            {
                if (!calendar->property(Properties::kDontPolishFontProperty).toBool())
                {
                    QFont font = widget->font();
                    font.setPixelSize(Metrics::kCalendarItemFontPixelSize);
                    widget->setFont(font);
                }

                if (!QnObjectCompanionManager::companion(calendar, kCalendarDelegateCompanion)
                    && !qobject_cast<QnCalendarWidget*>(calendar))
                {
                    QnObjectCompanionManager::attach(calendar,
                        new CalendarDelegateReplacement(view, calendar),
                        kCalendarDelegateCompanion);
                }
            }

            /* Fix for Qt 5.6 bug: item views don't reset their dropIndicatorRect: */
            connect(view, &QAbstractItemView::pressed, this,
                [view]()
                {
                    class QnAbstractItemViewCorrector: public QAbstractItemView
                    {
                        Q_DECLARE_PRIVATE(QAbstractItemView);

                    public:
                        void correctDropIndicatorRect()
                        {
                            Q_D(QAbstractItemView);
                            if (state() != DraggingState)
                                d->dropIndicatorRect = QRect();
                        }
                    };

                    static_cast<QnAbstractItemViewCorrector*>(view)->
                        correctDropIndicatorRect();
                });
        }
    }

    /*
     * Insert horizontal separator line into QInputDialog above its button box.
     * Since input dialogs are short-lived, don't bother with unpolishing.
     */
    if (auto inputDialog = qobject_cast<QInputDialog*>(widget))
        d->polishInputDialog(inputDialog);

    if (auto label = qobject_cast<QLabel*>(widget))
        QnObjectCompanion<QnLinkHoverProcessor>::install(label, kLinkHoverProcessorCompanion, true);

    if (kCustomizePopupShadows && popupToCustomizeShadow)
    {
        /* Create customized shadow: */
        if (auto shadow = QnObjectCompanion<QnPopupShadow>::install(popupToCustomizeShadow, kPopupShadowCompanion, true))
        {
            QnPaletteColor shadowColor = mainColor(Colors::kBase).darker(3);
            shadowColor.setAlphaF(0.5);
            shadow->setColor(shadowColor);
            shadow->setOffset(0, 10);
            shadow->setBlurRadius(20);
            shadow->setSpread(0);
        }
    }

    if (qobject_cast<QAbstractButton*>(widget) ||
        qobject_cast<QAbstractSlider*>(widget) ||
        qobject_cast<QGroupBox*>(widget) ||
        qobject_cast<QTabBar*>(widget) ||
        qobject_cast<QLabel*>(widget) ||
        isNonEditableComboBox(widget))
    {
        if (widget->focusPolicy() != Qt::NoFocus)
            widget->setFocusPolicy(Qt::TabFocus);
    }
}

void QnNxStyle::unpolish(QWidget* widget)
{
    widget->removeEventFilter(this);
    widget->disconnect(this);

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

    if ((widget->windowFlags() & Qt::ToolTip) == Qt::ToolTip)
        QnAbstractPropertyBackup::restore(widget, kContentsMarginsBackupId);

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

    if (auto calendar = qobject_cast<QCalendarWidget*>(widget))
        QnObjectCompanionManager::uninstall(calendar, kCalendarDelegateCompanion);

    if (kCustomizePopupShadows && popupWithCustomizedShadow)
        QnObjectCompanionManager::uninstall(popupWithCustomizedShadow, kPopupShadowCompanion);

    if (auto label = qobject_cast<QLabel*>(widget))
        QnObjectCompanionManager::uninstall(label, kLinkHoverProcessorCompanion);

    if (auto tabBar = qobject_cast<QTabBar*>(widget))
    {
        /* Remove hover events tracking: */
        tabBar->setProperty(kHoveredWidgetProperty, QVariant());
    }

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
    /* #QTBUG 18838 */
    /* Workaround for incorrectly updated hover state inside QGraphicsProxyWidget. */
    if (auto widget = qobject_cast<QWidget*>(object))
    {
        Q_D(QnNxStyle);
        if (auto proxy = d->graphicsProxyWidget(widget))
        {
            switch (event->type())
            {
                case QEvent::Leave:
                case QEvent::HoverLeave:
                {
                    if (d->lastProxiedWidgetUnderMouse == widget)
                        d->lastProxiedWidgetUnderMouse = nullptr;

                    break;
                }

                case QEvent::HoverMove:
                {
                    auto pos = static_cast<QHoverEvent*>(event)->pos();
                    auto child = proxy->widget()->childAt(widget->mapTo(proxy->widget(), pos));
                    if (child)
                        widget = child;
                    /* FALL THROUGH */
                }
                case QEvent::Enter:
                case QEvent::HoverEnter:
                {
                    if (d->lastProxiedWidgetUnderMouse == widget)
                        break;

                    if (d->lastProxiedWidgetUnderMouse)
                        d->lastProxiedWidgetUnderMouse->setAttribute(Qt::WA_UnderMouse, false);

                    widget->setAttribute(Qt::WA_UnderMouse, true);
                    d->lastProxiedWidgetUnderMouse = widget;
                    break;
                }

                default:
                    break;
            }
        }
    }

    /* QTabBar marks a tab as hovered even if a child widget is hovered above that tab.
     * To overcome this problem we process hover events and track hovered children: */
    if (auto tabBar = qobject_cast<QTabBar*>(object))
    {
        QWidget* hoveredWidget = nullptr;
        switch (event->type())
        {
            case QEvent::HoverMove:
            case QEvent::HoverEnter:
            {
                if (tabBar->rect().contains(static_cast<QHoverEvent*>(event)->pos()))
                {
                    hoveredWidget = tabBar->childAt(static_cast<QHoverEvent*>(event)->pos());
                    if (!hoveredWidget)
                        hoveredWidget = tabBar;
                }
                /* FALL THROUGH */
            }
            case QEvent::HoverLeave:
            {
                if (tabBar->property(kHoveredWidgetProperty).value<QPointer<QWidget>>() != hoveredWidget)
                {
                    tabBar->setProperty(kHoveredWidgetProperty,
                        QVariant::fromValue(QPointer<QWidget>(hoveredWidget)));
                    tabBar->update();
                }
                break;
            }

            default:
                break;
        }
    }
    /* Fix for Qt 5.6 bug: QDateTimeEdit doesn't calculate hovered subcontrol rect
     * which causes calendar dropdown button to not redraw properly. We simply update
     * entire control on each hover event, it's not optimal but will suffice for now: */
    else if (auto dateTime = qobject_cast<QDateTimeEdit*>(object))
    {
        switch (event->type())
        {
            case QEvent::HoverEnter:
            case QEvent::HoverMove:
            case QEvent::HoverLeave:
                dateTime->update();
                break;

            default:
                break;
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
    /* Fix for Qt 5.6 bug: QHeaderView doesn't resize stretch sections to minimum
     *  if quickly resized down.To overcome this problem we do it manually here: */
    if (auto header = qobject_cast<QHeaderView*>(object))
    {
        if (event->type() == QEvent::Resize)
        {
            auto updateSectionSizes =
                [header]()
                {
                    if (header->stretchSectionCount() > 0 && header->length() > header->width())
                    {
                        bool last = header->stretchLastSection();
                        int minumumSize = header->minimumSectionSize();

                        for (int i = header->count() - 1; i >= 0; --i)
                        {
                            if (header->isSectionHidden(i))
                                continue;

                            if (last || header->sectionResizeMode(i) == QHeaderView::Stretch)
                            {
                                header->resizeSection(i, minumumSize);
                                last = false;
                            }
                        }
                    }
                };

            executeDelayedParented(updateSectionSizes, 0, header);
        }
    }

    if (auto calendar = qobject_cast<QCalendarWidget*>(object))
    {
        if (event->type() == QEvent::PaletteChange)
        {
            QTextCharFormat header = calendar->headerTextFormat();
            header.setForeground(calendar->palette().windowText());
            calendar->setHeaderTextFormat(header);

            QTextCharFormat sunday = calendar->weekdayTextFormat(Qt::Sunday);
            sunday.setForeground(calendar->palette().brightText());
            calendar->setWeekdayTextFormat(Qt::Sunday, sunday);

            QTextCharFormat saturday = calendar->weekdayTextFormat(Qt::Saturday);
            saturday.setForeground(calendar->palette().brightText());
            calendar->setWeekdayTextFormat(Qt::Saturday, saturday);
        }
    }

    return base_type::eventFilter(object, event);
}

void QnNxStyle::setGroupBoxContentTopMargin(QGroupBox* box, int margin)
{
    NX_EXPECT(box);
    box->setProperty(style::Properties::kGroupBoxContentTopMargin, margin);
    box->setContentsMargins(groupBoxContentsMargins(box->isFlat(), box));
}

namespace {

void paintRectFrame(QPainter* painter, const QRectF& rect,
    const QColor& color, int width, int shift)
{
    QRectF outerRect = rect.adjusted(shift, shift, -shift, -shift);
    QRectF innerRect = outerRect.adjusted(width, width, -width, -width);

    if (width < 0) //< if outer frame
    {
        qSwap(outerRect, innerRect);
        width = -width;
    }

    const QRectF topRect(outerRect.left(), outerRect.top(), outerRect.width(), width);
    const QRectF leftRect(outerRect.left(), innerRect.top(), width, innerRect.height());
    const QRectF rightRect(innerRect.left() + innerRect.width(), innerRect.top(), width, innerRect.height());
    const QRectF bottomRect(outerRect.left(), innerRect.top() + innerRect.height(), outerRect.width(), width);

    const QBrush brush(color);
    painter->fillRect(topRect, brush);
    painter->fillRect(leftRect, brush);
    painter->fillRect(rightRect, brush);
    painter->fillRect(bottomRect, brush);
}

} // namespace

void QnNxStyle::paintCosmeticFrame(QPainter* painter, const QRectF& rect,
    const QColor& color, int width, int shift)
{
    if (width == 0)
        return;

    const PainterTransformScaleStripper scaleStripper(painter);
    const bool antialiasingRequired = scaleStripper.type() > QTransform::TxScale;
    const QnScopedPainterAntialiasingRollback antialiasingRollback(painter, antialiasingRequired);
    paintRectFrame(painter, scaleStripper.mapRect(rect), color, width, shift);
}
