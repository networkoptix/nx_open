#include "nx_style.h"
#include "nx_style_p.h"

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
#include <private/qfont_p.h>

#include <utils/common/scoped_painter_rollback.h>

using namespace style;

namespace
{
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
}

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
        return;

    case PE_PanelButtonCommand:
        {
            const bool pressed = option->state.testFlag(State_Sunken);
            const bool hovered = option->state.testFlag(State_MouseOver) ||
                                 option->state.testFlag(State_HasFocus);

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
        }
        return;
    case PE_PanelButtonTool:
        {
            const QTabBar *tabBar = nullptr;
            TabShape shape = TabShape::Default;
            if (widget)
            {
                tabBar = qobject_cast<const QTabBar*>(widget->parent());
                shape = tabShape(tabBar);
            }

            const bool pressed = option->state.testFlag(State_Sunken);
            const bool hovered = option->state.testFlag(State_MouseOver) ||
                                 option->state.testFlag(State_HasFocus);

            QRect rect = option->rect;

            QnPaletteColor mainColor = findColor(option->palette.window().color());
            if (tabBar)
            {
                switch (shape)
                {
                case TabShape::Default:
                    mainColor = mainColor.lighter(3);
                    rect.adjust(0, 0, 0, -1);
                    break;
                case TabShape::Compact:
                    rect.adjust(0, 0, 0, -1);
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
            }
        }
        return;

    case PE_PanelLineEdit:
    case PE_FrameLineEdit:
        {
            if (widget && (qobject_cast<const QSpinBox *>(widget->parentWidget()) ||
                           qobject_cast<const QComboBox *>(widget->parentWidget())))
            {
                // Do not draw panel for the internal line edit of certain widgets.
                return;
            }

            painter->save();

            QnPaletteColor mainColor = findColor(option->palette.color(QPalette::Base));

            QPen pen(mainColor.darker(3));
            painter->setPen(pen);

            QColor base = mainColor.darker(2);
            if (option->state & State_MouseOver)
                base = mainColor.darker(3);

            painter->setBrush(base);
            painter->setRenderHint(QPainter::Antialiasing);

            painter->drawRoundedRect(QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5), 1, 1);

            if (option->state & State_HasFocus)
            {
                painter->drawLine(option->rect.left() + 1, option->rect.top() + 1,
                                  option->rect.right() - 1, option->rect.top() + 1);
                painter->drawLine(option->rect.left() + 1, option->rect.top() + 1,
                                  option->rect.left() + 1, option->rect.bottom() - 1);
            }

            painter->restore();
        }
        return;

    /**
     * Hover marker is drawn in PE_PanelItemViewRow.
     * Selection marker is drawn over later in PE_PanelItemViewItem,
     *  except selection marker in tree view branch area which is drawn in PE_PanelItemViewRow as well.
     */
    case PE_PanelItemViewRow:
        if (const QStyleOptionViewItem *item = qstyleoption_cast<const QStyleOptionViewItem *>(option))
        {
            /* Markers can be semi-transparent, so we draw all layers on top of each other. */

            /* Draw alternate row background if requested: */
            if (item->features.testFlag(QStyleOptionViewItem::Alternate))
                painter->fillRect(item->rect, option->palette.alternateBase());

            /* Draw hover marker if needed: */
            if (item->state.testFlag(State_MouseOver))
                painter->fillRect(item->rect, option->palette.midlight());

            /* Draw selection marker if needed: */
            if (item->state.testFlag(State_Selected))
                painter->fillRect(item->rect, option->palette.highlight());

            return;
        }
        break;

    case PE_PanelItemViewItem:
        //TODO #vkutin Complete refinement of this code when restyling table views
        if (const QStyleOptionViewItem *item = qstyleoption_cast<const QStyleOptionViewItem *>(option))
        {
            bool selected = item->state.testFlag(State_Selected);

            /* Handle here entire row hovering in table views: */
            int hoveredRow = -1;
            if (widget)
            {
                QVariant value = widget->property(Properties::kHoveredRowProperty);
                if (value.isValid())
                    hoveredRow = value.toInt();
            }

            bool hovered = item->index.row() == hoveredRow;

            QnPaletteColor fillColor;
            if (selected)
            {
                fillColor = findColor(option->palette.highlight().color());
                if (hovered)
                    fillColor = fillColor.lighter(1);
            }
            else if (hovered)
            {
                fillColor = findColor(option->palette.midlight().color()).darker(1);
            }

            if (fillColor.isValid())
                painter->fillRect(item->rect, fillColor.color());

            return;
        }
        break;

    case PE_FrameGroupBox:
        return;

    case PE_IndicatorCheckBox:
    case PE_IndicatorViewItemCheck:
        d->drawCheckBox(painter, option, widget);
        return;

    case PE_IndicatorRadioButton:
        d->drawRadioButton(painter, option, widget);
        return;

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

            if (shape == TabShape::Default)
            {
                painter->fillRect(rect, mainColor);

                painter->setPen(mainColor.darker(1));
                painter->drawLine(rect.topLeft(), rect.topRight());

                painter->setPen(mainColor.darker(3));
                painter->drawLine(rect.bottomLeft(), rect.bottomRight());
            }
            else
            {
                painter->setPen(mainColor.lighter(2));
                painter->drawLine(rect.bottomLeft(), rect.bottomRight());
            }
        }
        return;

    case PE_PanelMenu:
        {
            const int radius = dp(3);
            QnPaletteColor backgroundColor = findColor(option->palette.color(QPalette::Window));
            painter->save();

            painter->setPen(backgroundColor.darker(3));
            painter->setBrush(QBrush(backgroundColor));
            painter->setRenderHint(QPainter::Antialiasing);

            painter->drawRoundedRect(option->rect, radius, radius);

            painter->restore();
        }
        return;

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
        }
        return;

    case PE_FrameMenu:
        return;

    case PE_Frame:
        return;

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
    Q_D(const QnNxStyle);

    switch (control)
    {
    case CC_ComboBox:
        if (const QStyleOptionComboBox *comboBox =
             qstyleoption_cast<const QStyleOptionComboBox *>(option))
        {
            painter->save();

            if (comboBox->editable)
            {
                proxy()->drawPrimitive(PE_FrameLineEdit, comboBox, painter, widget);

                QRect buttonRect = subControlRect(control, option, SC_ComboBoxArrow, widget);

                QnPaletteColor mainColor = findColor(comboBox->palette.color(QPalette::Button));
                QnPaletteColor buttonColor;

                if (comboBox->state.testFlag(State_On))
                {
                    buttonColor = mainColor.darker(1);
                }
                else if (comboBox->activeSubControls.testFlag(SC_ComboBoxArrow))
                {
                    buttonColor = mainColor.lighter(1);
                }

                if (buttonColor.isValid())
                {
                    painter->setBrush(QBrush(buttonColor));
                    painter->setPen(Qt::NoPen);
                    painter->setRenderHint(QPainter::Antialiasing);
                    painter->drawRoundedRect(buttonRect, 2, 2);
                    painter->drawRect(buttonRect.adjusted(0, 0, -buttonRect.width() / 2, 0));
                    painter->setRenderHint(QPainter::Antialiasing, false);
                }
            }
            else
            {
                QStyleOptionButton buttonOption;
                buttonOption.QStyleOption::operator=(*comboBox);
                buttonOption.rect = comboBox->rect;
                buttonOption.state = comboBox->state & (State_Enabled | State_MouseOver | State_HasFocus | State_KeyboardFocusChange);
                if (comboBox->state & State_On)
                    buttonOption.state |= State_Sunken;
                proxy()->drawPrimitive(PE_PanelButtonCommand, &buttonOption, painter, widget);
            }

            if (comboBox->subControls.testFlag(SC_ComboBoxArrow))
            {
                QRectF rect = subControlRect(CC_ComboBox, comboBox, SC_ComboBoxArrow, widget);
                drawArrow(Down, painter, rect.translated(0, -1), option->palette.color(QPalette::Text));
            }

            painter->restore();
            return;
        }
        break;

    case CC_Slider:
        if (const QStyleOptionSlider *slider = qstyleoption_cast<const QStyleOptionSlider*>(option))
        {
            QRectF grooveRect = proxy()->subControlRect(CC_Slider, option, SC_SliderGroove, widget);
            QRectF handleRect = proxy()->subControlRect(CC_Slider, option, SC_SliderHandle, widget);

            const bool horizontal = slider->orientation == Qt::Horizontal;
            const bool hovered = slider->state.testFlag(State_MouseOver) ||
                                 option->state.testFlag(State_HasFocus);

            QnPaletteColor mainDark = findColor(slider->palette.color(QPalette::Window));
            QnPaletteColor mainLight = findColor(slider->palette.color(QPalette::WindowText));

            QnScopedPainterPenRollback penRollback(painter);
            QnScopedPainterBrushRollback brushRollback(painter);

            if (slider->subControls.testFlag(SC_SliderGroove))
            {
                QRectF grooveDrawRect = grooveRect.adjusted(0.5, 0.5, -0.5, -0.5); /* to include border, antialiased */
                qreal radius = ((horizontal ? grooveDrawRect.height() : grooveDrawRect.width())) * 0.5 ;

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

            return;
        }
        break;

    case CC_GroupBox:
        if (const QStyleOptionGroupBox *groupBox =
                qstyleoption_cast<const QStyleOptionGroupBox*>(option))
        {
            painter->save();

            const bool flat = groupBox->features.testFlag(QStyleOptionFrame::Flat);

            QRect labelRect = subControlRect(CC_GroupBox, groupBox, SC_GroupBoxLabel, widget);

            if (groupBox->subControls.testFlag(SC_GroupBoxFrame))
            {
                QRect rect = subControlRect(CC_GroupBox, groupBox, SC_GroupBoxFrame, widget).adjusted(0, 0, -1, -1);

                QnPaletteColor mainColor = findColor(option->palette.color(QPalette::Dark));

                if (flat)
                {
                    painter->setPen(mainColor.lighter(2));
                    painter->drawLine(rect.topLeft(), rect.topRight());
                }
                else
                {
                    const int radius = dp(2);
                    const int spacing = dp(8);
                    const qreal penWidth = 1.0;

                    QPainterPath path;
                    path.moveTo(labelRect.left() - spacing, rect.top());
                    path.lineTo(rect.left() + radius, rect.top());
                    path.quadTo(rect.left(), rect.top(), rect.left(), rect.top() + radius);
                    path.lineTo(rect.left(), rect.bottom() - radius);
                    path.quadTo(rect.left(), rect.bottom(), rect.left() + radius, rect.bottom());
                    path.lineTo(rect.right() - radius, rect.bottom());
                    path.quadTo(rect.right(), rect.bottom(), rect.right(), rect.bottom() - radius);
                    path.lineTo(rect.right(), rect.top() + radius);
                    path.quadTo(rect.right(), rect.top(), rect.right() - radius, rect.top());
                    path.lineTo(labelRect.right() + spacing, rect.top());

                    painter->save();
                    painter->translate(penWidth / 2, penWidth / 2);
                    painter->setPen(mainColor);
                    painter->setRenderHint(QPainter::Antialiasing);
                    painter->drawPath(path);
                    painter->restore();
                }
            }

            if (groupBox->subControls.testFlag(SC_GroupBoxLabel))
            {
                int flags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextHideMnemonic;

                if (flat)
                {
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

                        drawItemText(painter, rect, flags,
                                     groupBox->palette, groupBox->state.testFlag(QStyle::State_Enabled),
                                     text, QPalette::Text);

                        rect.setLeft(rect.left() + QFontMetrics(font).size(flags, text).width());
                    }

                    if (!detailText.isEmpty())
                    {
                        drawItemText(painter, rect, Qt::AlignCenter | Qt::TextHideMnemonic,
                                     groupBox->palette, groupBox->state.testFlag(QStyle::State_Enabled),
                                     detailText, QPalette::WindowText);
                    }
                }
                else
                {
                    drawItemText(painter, labelRect, flags,
                                 groupBox->palette, groupBox->state.testFlag(QStyle::State_Enabled),
                                 groupBox->text, QPalette::WindowText);
                }
            }

            if (groupBox->subControls.testFlag(SC_GroupBoxCheckBox))
            {
                QStyleOption opt = *option;
                opt.rect = subControlRect(CC_GroupBox, groupBox, SC_GroupBoxCheckBox, widget);
                opt.state |= State_Item;

                d->drawSwitch(painter, &opt, widget);
            }

            painter->restore();

            return;
        }
        break;

    case CC_SpinBox:
        if (const QStyleOptionSpinBox *spinBox =
                qstyleoption_cast<const QStyleOptionSpinBox*>(option))
        {
            if (option->subControls & SC_SpinBoxFrame)
                proxy()->drawPrimitive(PE_PanelLineEdit, option, painter, widget);

            auto drawArrowButton = [&](QStyle::SubControl subControl)
            {
                QRect buttonRect = subControlRect(control, spinBox, subControl, widget);

                QnPaletteColor mainColor = findColor(spinBox->palette.color(QPalette::Button));
                QColor buttonColor;

                bool up = (subControl == SC_SpinBoxUp);
                bool enabled = spinBox->stepEnabled.testFlag(up ? QSpinBox::StepUpEnabled : QSpinBox::StepDownEnabled);

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
                          subControlRect(control, option, subControl, widget),
                          option->palette.color(enabled ? QPalette::Active : QPalette::Disabled, QPalette::Text));
            };

            if (option->subControls.testFlag(SC_SpinBoxUp))
                drawArrowButton(SC_SpinBoxUp);

            if (option->subControls.testFlag(SC_SpinBoxDown))
                drawArrowButton(SC_SpinBoxDown);

            return;
        }
        break;

    case CC_ScrollBar:
        if (const QStyleOptionSlider *scrollBar =
                qstyleoption_cast<const QStyleOptionSlider*>(option))
        {
            QRect scrollBarSlider = proxy()->subControlRect(control, scrollBar, SC_ScrollBarSlider, widget);
            QRect scrollBarGroove = proxy()->subControlRect(control, scrollBar, SC_ScrollBarGroove, widget);

            QBrush grooveBrush = scrollBar->palette.dark();

            QnPaletteColor mainSliderColor = findColor(scrollBar->palette.color(QPalette::Midlight));
            QColor sliderColor = mainSliderColor;

            if (scrollBar->state.testFlag(State_Sunken))
            {
                sliderColor = mainSliderColor.lighter(1);
            }
            else if (scrollBar->state.testFlag(State_MouseOver))
            {
                if (scrollBar->activeSubControls.testFlag(SC_ScrollBarSlider))
                    sliderColor = mainSliderColor.lighter(2);
                else
                    sliderColor = mainSliderColor.lighter(1);
            }

            if (scrollBar->subControls & SC_ScrollBarGroove)
                painter->fillRect(scrollBarGroove, grooveBrush);

            if (scrollBar->subControls & SC_ScrollBarSlider)
                painter->fillRect(scrollBarSlider, sliderColor);

            return;
        }
        break;

    case CC_ToolButton:
        if (const QStyleOptionToolButton* button =
                qstyleoption_cast<const QStyleOptionToolButton*>(option))
        {
            QStyleOptionToolButton opt(*button);
            opt.features &= ~QStyleOptionToolButton::HasMenu;
            base_type::drawComplexControl(control, &opt, painter, widget);
            return;
        }
        break;

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
        if (const QStyleOptionFrame *frame =
                qstyleoption_cast<const QStyleOptionFrame *>(option))
        {
            switch (frame->frameShape)
            {
            case QFrame::Box:
                {
                    QnPaletteColor mainColor = findColor(option->palette.color(QPalette::Shadow)).darker(1);
                    QnScopedPainterPenRollback penRollback(painter, mainColor.color());
                    painter->drawRect(frame->rect.adjusted(0, 0, -1, -1));
                }
                return;

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
                        rect.setWidth(frame->lineWidth);
                        painter->fillRect(rect, firstColor);

                        if (secondColor.isValid())
                        {
                            rect.moveLeft(rect.left() + rect.width());
                            painter->fillRect(rect, secondColor);
                        }
                    }
                }
                return;

            default:
                break;
            }
        }
        break;

    case CE_ComboBoxLabel:
        if (const QStyleOptionComboBox *comboBox =
                qstyleoption_cast<const QStyleOptionComboBox*>(option))
        {
            QStyleOptionComboBox opt = *comboBox;
            opt.rect.setLeft(opt.rect.left() + dp(6));
            base_type::drawControl(element, &opt, painter, widget);
            return;
        }
        break;

    case CE_CheckBoxLabel:
    case CE_RadioButtonLabel:
        if (const QStyleOptionButton *button =
                qstyleoption_cast<const QStyleOptionButton*>(option))
        {
            QStyleOptionButton opt = *button;
            opt.palette.setColor(QPalette::WindowText, d->checkBoxColor(option, element == CE_RadioButtonLabel));
            base_type::drawControl(element, &opt, painter, widget);
            return;
        }
        break;

    case CE_TabBarTabShape:
        if (const QStyleOptionTab *tab =
                qstyleoption_cast<const QStyleOptionTab*>(option))
        {
            TabShape shape = tabShape(widget);

            switch (shape)
            {
            case TabShape::Compact:
                return;

            case TabShape::Default:
                if (!tab->state.testFlag(State_Selected) &&
                    (tab->state.testFlag(State_MouseOver) || tab->state.testFlag(State_HasFocus)))
                {
                    QnPaletteColor mainColor = findColor(
                            option->palette.window().color()).lighter(3);

                    painter->fillRect(tab->rect.adjusted(0, 0, 0, -1), mainColor);

                    QnScopedPainterPenRollback penRollback(
                            painter, QPen(mainColor.lighter(2)));
                    painter->drawLine(tab->rect.topLeft(), tab->rect.topRight());
                }
                return;

            case TabShape::Rectangular:
                {
                    QnPaletteColor mainColor = findColor(
                            option->palette.window().color()).lighter(1);

                    QColor color = mainColor;

                    if (tab->state.testFlag(State_Selected))
                    {
                        color = option->palette.midlight().color();
                    }
                    else if (tab->state.testFlag(State_MouseOver) ||
                             tab->state.testFlag(State_HasFocus))
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
                }
                return;
            }
        }
        break;

    case CE_TabBarTabLabel:
        if (const QStyleOptionTab *tab =
                qstyleoption_cast<const QStyleOptionTab*>(option))
        {
            TabShape shape = tabShape(widget);
            int textFlags = Qt::TextHideMnemonic;

            QColor color;

            QRect textRect = tab->rect;

            if (shape == TabShape::Rectangular)
            {
                textFlags |= Qt::AlignLeft | Qt::AlignVCenter;

                if (tab->state.testFlag(QStyle::State_Selected))
                    color = tab->palette.text().color();
                else
                    color = tab->palette.light().color();

                int hspace = pixelMetric(PM_TabBarTabHSpace, option, widget);
                textRect.adjust(hspace, 0, -hspace, 0);
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
                }
                else
                {
                    rect.setBottom(textRect.bottom() - 1);
                    rect.setTop(rect.bottom() - 1);
                }

                if (tab->state.testFlag(QStyle::State_Selected))
                {
                    color = tab->palette.highlight().color();
                    painter->fillRect(rect, color);
                }
                else if (tab->state.testFlag(State_MouseOver) ||
                         tab->state.testFlag(State_HasFocus))
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
        }
        return;

    case CE_HeaderSection:
        if (const QStyleOptionHeader *header =
                qstyleoption_cast<const QStyleOptionHeader*>(option))
        {
            if (header->state.testFlag(State_MouseOver) || header->state.testFlag(State_HasFocus))
            {
                QColor color = findColor(header->palette.midlight().color()).darker(1);
                color.setAlphaF(0.2);
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

    case CE_HeaderEmptyArea:
        if (const QStyleOptionHeader *header =
                qstyleoption_cast<const QStyleOptionHeader*>(option))
        {
            painter->fillRect(header->rect, header->palette.window());
            return;
        }
        break;

    case CE_MenuItem:
        if (const QStyleOptionMenuItem *menuItem =
                qstyleoption_cast<const QStyleOptionMenuItem*>(option))
        {
            if (menuItem->menuItemType == QStyleOptionMenuItem::Separator)
            {
                painter->save();

                int y = menuItem->rect.top() + menuItem->rect.height() / 2;
                painter->setPen(findColor(menuItem->palette.color(QPalette::Window)).darker(2));
                painter->drawLine(Metrics::kMenuItemHPadding, y,
                                  menuItem->rect.right() - Metrics::kMenuItemHPadding, y);

                painter->restore();
                break;
            }

            painter->save();

            bool enabled = menuItem->state.testFlag(State_Enabled);
            bool selected = enabled && menuItem->state.testFlag(State_Selected);

            QColor textColor = menuItem->palette.color(QPalette::WindowText);
            QColor backgroundColor = menuItem->palette.color(QPalette::Window);
            QColor shortcutColor = menuItem->palette.color(QPalette::Midlight);

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

            if (menuItem->checked)
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

            painter->restore();
            return;
        }
        break;

    case CE_ProgressBarGroove:
        if (const QStyleOptionProgressBar *progressBar =
                qstyleoption_cast<const QStyleOptionProgressBar *>(option))
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

    case CE_ProgressBarContents:
        if (const QStyleOptionProgressBar *progressBar =
                qstyleoption_cast<const QStyleOptionProgressBar *>(option))
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

    case CE_ProgressBarLabel:
        if (const QStyleOptionProgressBar *progressBar =
                qstyleoption_cast<const QStyleOptionProgressBar *>(option))
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

    case CE_PushButtonLabel:
        {
            if (isCheckableButton(option))
            {
                /* Calculate minimal label width: */
                const QStyleOptionButton* buttonOption = static_cast<const QStyleOptionButton*>(option); /* isCheckableButton()==true guarantees type safety */
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
                newOpt.rect.setWidth(Metrics::kSwitchSize.width());
                newOpt.rect.moveRight(option->rect.right() - Metrics::kSwitchMargin);
                drawSwitch(painter, &newOpt, widget);
                return;
            }
        }
        break;

    case CE_Splitter:
        {
            painter->fillRect(option->rect, option->palette.shadow());
        }
        break;

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
        if (const QStyleOptionSlider *slider =
            qstyleoption_cast<const QStyleOptionSlider*>(option))
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
                }
                break;

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
                }
                break;

            default:
                break;
            }
        }
        break;

    case CC_ComboBox:
        if (const QStyleOptionComboBox *comboBox =
                qstyleoption_cast<const QStyleOptionComboBox*>(option))
        {
            switch (subControl)
            {
            case SC_ComboBoxArrow:
                rect = QRect(comboBox->rect.right() - comboBox->rect.height(), 0,
                             comboBox->rect.height(), comboBox->rect.height());
                rect.adjust(0, 1, 0, -1);
                break;

            case SC_ComboBoxEditField:
                {
                    int frameWidth = pixelMetric(PM_ComboBoxFrameWidth, option, widget);
                    rect = comboBox->rect;
                    rect.setRight(rect.right() - rect.height());
                    rect.adjust(frameWidth, frameWidth, 0, -frameWidth);
                }
                break;

            default:
                break;
            }
        }
        break;

    case CC_SpinBox:
        if (const QStyleOptionSpinBox *spinBox =
                qstyleoption_cast<const QStyleOptionSpinBox*>(option))
        {
            switch (subControl)
            {
            case SC_SpinBoxEditField:
                {
                    int frameWidth = pixelMetric(PM_SpinBoxFrameWidth, option, widget);
                    int buttonWidth = subControlRect(control, option, SC_SpinBoxDown, widget).width();
                    rect = spinBox->rect;
                    rect.setRight(rect.right() - buttonWidth);
                    rect.adjust(frameWidth, frameWidth, 0, -frameWidth);
                }
                break;

            case SC_SpinBoxDown:
            case SC_SpinBoxUp:
                rect.adjust(0, 0, 0, 1);
                break;

            default:
                break;
            }
        }
        break;

    case CC_GroupBox:
        if (const QStyleOptionGroupBox *groupBox =
                qstyleoption_cast<const QStyleOptionGroupBox*>(option))
        {
            switch (subControl)
            {
            case SC_GroupBoxFrame:
                if (groupBox->features.testFlag(QStyleOptionFrame::Flat))
                {
                    QRect labelRect = subControlRect(CC_GroupBox, option, SC_GroupBoxLabel, widget);
                    rect.setTop(labelRect.bottom() + dp(6));
                }
                break;

            case SC_GroupBoxLabel:
                if (groupBox->features.testFlag(QStyleOptionFrame::Flat))
                {
                    if (widget)
                    {
                        QString text = groupBox->text;
                        QString detailText;

                        int splitPos = text.indexOf(QLatin1Char('\t'));
                        if (splitPos >= 0)
                        {
                            detailText = text.mid(splitPos + 1);
                            text = text.left(splitPos);
                        }

                        QFont font = widget->font();
                        font.setPixelSize(font.pixelSize() + 2);
                        font.setWeight(QFont::DemiBold);

                        int flags = Qt::AlignTop |
                                    Qt::AlignLeft |
                                    Qt::TextHideMnemonic;

                        QSize size = QFontMetrics(font).size(flags, text);

                        if (!detailText.isEmpty())
                        {
                            QSize detailSize = QFontMetrics(widget->font())
                                               .size(flags, detailText);
                            size.rwidth() += detailSize.width();
                        }

                        rect = QRect(QPoint(), size);
                    }

                    rect.moveLeft(0);
                    rect.setHeight(dp(18));
                }
                else
                {
                    rect.moveLeft(dp(16));
                }
                break;

            case SC_GroupBoxCheckBox:
                if (groupBox->features.testFlag(QStyleOptionFrame::Flat))
                {
                    QRect boundRect = subControlRect(CC_GroupBox, option, SC_GroupBoxLabel, widget);
                    boundRect.setRight(option->rect.right());
                    rect = alignedRect(Qt::LeftToRight, Qt::AlignRight | Qt::AlignVCenter,
                                       Metrics::kSwitchSize, boundRect);
                }
                break;

            case SC_GroupBoxContents:
                if (groupBox->features.testFlag(QStyleOptionFrame::Flat))
                {
                    QRect labelRect = subControlRect(CC_GroupBox, option, SC_GroupBoxLabel, widget);
                    rect.setTop(labelRect.bottom() + dp(10));
                }
                break;

            default:
                break;
            }
        }
        break;

    case CC_ScrollBar:
        if (const QStyleOptionSlider *scrollBar =
                qstyleoption_cast<const QStyleOptionSlider*>(option))
        {
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
        if (!widget || !widget->parent() || !qobject_cast<const QAbstractItemView*>(widget->parent()->parent()))
            //TODO #vkutin See why this ugly "dp(6)" is here and not somewhere else
            return base_type::subElementRect(subElement, option, widget).adjusted(dp(6), 0, 0, 0);
        break;

    case SE_PushButtonLayoutItem:
        if (qobject_cast<const QDialogButtonBox *>(widget))
        {
            const int shift = dp(16);
            return option->rect.adjusted(-shift, -shift, shift, shift);
        }
        break;

    case SE_PushButtonFocusRect:
        return option->rect;

    case SE_ProgressBarGroove:
        if (const QStyleOptionProgressBar *progressBar =
                qstyleoption_cast<const QStyleOptionProgressBar *>(option))
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

    case SE_ProgressBarContents:
        if (const QStyleOptionProgressBar *progressBar =
                qstyleoption_cast<const QStyleOptionProgressBar *>(option))
        {
            return subElementRect(SE_ProgressBarGroove, progressBar, widget).adjusted(1, 1, -1, -1);
        }
        break;

    case SE_ProgressBarLabel:
        if (const QStyleOptionProgressBar *progressBar =
                qstyleoption_cast<const QStyleOptionProgressBar *>(option))
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

    case SE_TabWidgetTabBar:
        if (const QTabWidget* tabWidget = qobject_cast<const QTabWidget*>(widget))
        {
            int hspace = pixelMetric(PM_TabBarTabHSpace, option, widget) / 2;

            QRect rect = base_type::subElementRect(subElement, option, widget);

            if (tabShape(tabWidget->tabBar()) == TabShape::Compact)
                rect.adjust(-hspace, 0, -hspace, 0);
            else
                rect.adjust(hspace, 0, hspace, 0);

            if (rect.right() > option->rect.right())
                rect.setRight(option->rect.right());

            return rect;
        }
        break;

    case SE_TabBarTabLeftButton:
        if (const QStyleOptionTab* tabBar = qstyleoption_cast<const QStyleOptionTab*>(option))
        {
            QSize size = tabBar->leftButtonSize;
            size.setHeight(std::min(size.height(), tabBar->rect.height()));
            return aligned(size, tabBar->rect, Qt::AlignLeft | Qt::AlignVCenter);
        }
        break;

    case SE_TabBarTabRightButton:
        if (const QStyleOptionTab* tabBar = qstyleoption_cast<const QStyleOptionTab*>(option))
        {
            QSize size = tabBar->rightButtonSize;
            size.setHeight(std::min(size.height(), tabBar->rect.height()));
            return aligned(size, tabBar->rect.adjusted(0, 0, -1, 0), Qt::AlignRight | Qt::AlignVCenter);
        }
        break;

    case SE_ItemViewItemCheckIndicator:
        if (const QStyleOptionViewItem *item = qstyleoption_cast<const QStyleOptionViewItem *>(option))
        {
            if (item->state.testFlag(State_On) || item->state.testFlag(State_Off))
                return alignedRect(Qt::LeftToRight, Qt::AlignCenter, Metrics::kSwitchSize, option->rect);
        }
        break;

    case SE_HeaderArrow:
        if (const QStyleOptionHeader *header = qstyleoption_cast<const QStyleOptionHeader *>(option))
        {
            QSize size(Metrics::kSortIndicatorSize, Metrics::kSortIndicatorSize);
            QRect rect = header->rect.adjusted(Metrics::kStandardPadding, 0, -Metrics::kStandardPadding, 0);
            Qt::Alignment alignment = static_cast<Qt::Alignment>(styleHint(SH_Header_ArrowAlignment, header, widget));

            if (alignment.testFlag(Qt::AlignRight))
            {
                QRect labelRect = subElementRect(SE_HeaderLabel, option, widget);
                int margin = pixelMetric(PM_HeaderMargin, header, widget);
                rect.setLeft(labelRect.right() + margin);
            }

            return alignedRect(Qt::LeftToRight, Qt::AlignLeft | (alignment & Qt::AlignVertical_Mask), size, rect);
        }
        break;

    case SE_HeaderLabel:
        if (const QStyleOptionHeader *header = qstyleoption_cast<const QStyleOptionHeader *>(option))
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

    default:
        break;
    } // switch

    return base_type::subElementRect(subElement, option, widget);
}

int QnNxStyle::pixelMetric(
        PixelMetric metric,
        const QStyleOption *option,
        const QWidget *widget) const
{
    switch (metric)
    {
    case PM_ButtonMargin:
        return dp(10);

    case PM_ButtonShiftVertical:
    case PM_ButtonShiftHorizontal:
    case PM_TabBarTabShiftVertical:
    case PM_TabBarTabShiftHorizontal:
        return 0;

    case PM_DefaultFrameWidth:
        return 0;

    case PM_ExclusiveIndicatorWidth:
    case PM_ExclusiveIndicatorHeight:
        return Metrics::kExclusiveIndicatorSize;
    case PM_IndicatorWidth:
    case PM_IndicatorHeight:
        return Metrics::kCheckIndicatorSize;

    case PM_FocusFrameHMargin:
    case PM_FocusFrameVMargin:
        return dp(1);

    case PM_HeaderMarkSize:
        return Metrics::kSortIndicatorSize;
    case PM_HeaderMargin:
        return dp(6);

    case PM_LayoutTopMargin:
    case PM_LayoutBottomMargin:
    case PM_LayoutLeftMargin:
    case PM_LayoutRightMargin:
        return dp(0);
    case PM_LayoutHorizontalSpacing:
    case PM_LayoutVerticalSpacing:
        return dp(8);

    case PM_MenuVMargin:
        return dp(2);
    case PM_SubMenuOverlap:
        return 0;

    case PM_SliderControlThickness:
        return dp(16);
    case PM_SliderThickness:
        return dp(18);
    case PM_SliderLength:
        if (option && option->styleObject)
        {
            bool ok(false);
            int result = option->styleObject->property(Properties::kSliderLength).toInt(&ok);
            if (ok && result >= 0)
                return result;
        }
        return dp(16);

    case PM_ScrollBarExtent:
        return dp(8);
    case PM_ScrollBarSliderMin:
        return dp(8);

    case PM_SplitterWidth:
        return dp(1);

    case PM_TabBarTabHSpace:
    case PM_TabBarTabVSpace:
        return tabShape(widget) == TabShape::Rectangular ? dp(8) : dp(20);

    case PM_ToolBarIconSize:
        return dp(32); // TODO #vkutin Remove dp() from all places where it's not needed

    case PM_TabCloseIndicatorWidth:
    case PM_TabCloseIndicatorHeight:
        return dp(24);

    case PM_TabBarScrollButtonWidth:
        return std::min(dp(36), widget->height() + 1);

    default:
        break;
    }

    return base_type::pixelMetric(metric, option, widget);
}

QSize QnNxStyle::sizeFromContents(
        ContentsType type,
        const QStyleOption *option,
        const QSize &size,
        const QWidget *widget) const
{
    switch (type)
    {
    case CT_PushButton:
        {
            QSize switchSize;
            if (isCheckableButton(option))
                switchSize = Metrics::kSwitchSize + QSize(Metrics::kSwitchMargin, 0);

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
        /* Default processing for normal spin boxes: */
        break;
    }

    case CT_ComboBox:
    {
        bool hasArrow = false;
        if (const QStyleOptionComboBox *comboBox = qstyleoption_cast<const QStyleOptionComboBox *>(option))
            hasArrow = comboBox->subControls.testFlag(SC_ComboBoxArrow);

        int height = qMax(size.height(), Metrics::kButtonHeight);

        int hMargin = pixelMetric(PM_ButtonMargin, option, widget);
        int width = qMax(Metrics::kMinimumButtonWidth,
                         size.width() + hMargin + (hasArrow ? height : hMargin));

        return QSize(width, height);
    }

    case CT_TabBarTab:
        if (qstyleoption_cast<const QStyleOptionTab*>(option))
        {
            TabShape shape = tabShape(widget);

            const int kRoundedSize = dp(36);
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

    case CT_MenuItem:
        return QSize(size.width() + dp(24) + 2 * Metrics::kMenuItemHPadding,
                     size.height() + 2 * Metrics::kMenuItemVPadding);

    case CT_HeaderSection:
        if (const QStyleOptionHeader *header = qstyleoption_cast<const QStyleOptionHeader *>(option))
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

    case CT_ItemViewItem:
        {
            QSize sz = base_type::sizeFromContents(type, option, size, widget);
            sz.setHeight(qMax(sz.height(), Metrics::kViewRowHeight));
            return sz;
        }

    default:
        break;
    }

    return base_type::sizeFromContents(type, option, size, widget);
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
        if (const QStyleOptionGroupBox *groupBox =
                qstyleoption_cast<const QStyleOptionGroupBox*>(option))
        {
            if (groupBox->features & QStyleOptionFrame::Flat)
                return int(groupBox->palette.color(QPalette::Text).rgba());
            else
                return int(groupBox->palette.color(QPalette::WindowText).rgba());
        }
        break;

    case SH_Menu_MouseTracking:
    case SH_ComboBox_ListMouseTracking:
        return 1;
    case SH_ComboBox_PopupFrameStyle:
        return QFrame::NoFrame;
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

void QnNxStyle::polish(QWidget *widget)
{
    base_type::polish(widget);

    if (qobject_cast<QPushButton*>(widget) ||
        qobject_cast<QToolButton*>(widget))
    {
        if (!widget->property(Properties::kDontPolishFontProperty).toBool())
        {
            QFont font = widget->font();
            font.setWeight(QFont::DemiBold);
            widget->setFont(font);
        }
        widget->setAttribute(Qt::WA_Hover);
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

    if (qobject_cast<QLineEdit*>(widget) ||
        qobject_cast<QComboBox*>(widget) ||
        qobject_cast<QSpinBox*>(widget) ||
        qobject_cast<QCheckBox*>(widget) ||
        qobject_cast<QGroupBox*>(widget) ||
        qobject_cast<QRadioButton*>(widget) ||
        qobject_cast<QSlider*>(widget) ||
        qobject_cast<QScrollBar*>(widget))
    {
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
        widget->setAttribute(Qt::WA_Hover);
    }

    if (QAbstractItemView *view = qobject_cast<QAbstractItemView *>(widget))
    {
        view->viewport()->setAttribute(Qt::WA_Hover);
    }

    if (qobject_cast<QMenu*>(widget))
    {
        widget->setAttribute(Qt::WA_TranslucentBackground);
#ifdef Q_OS_WIN
        widget->setWindowFlags(widget->windowFlags() | Qt::FramelessWindowHint);
#endif
    }
}

void QnNxStyle::unpolish(QWidget *widget)
{
    if (qobject_cast<QAbstractButton*>(widget) ||
        qobject_cast<QHeaderView*>(widget) ||
        qobject_cast<QTabBar*>(widget))
    {
        if (!widget->property(Properties::kDontPolishFontProperty).toBool())
            widget->setFont(qApp->font());
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
    }

    return QString();
}
