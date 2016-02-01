#include "nx_style.h"
#include "nx_style_p.h"

#include <QPainter>
#include <QStyleOption>
#include <QStyleOptionButton>
#include <QPushButton>
#include <QToolButton>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QMenu>
#include <QDialogButtonBox>
#include <QAbstractItemView>
#include <private/qfont_p.h>
#include <QtMath>

#include <QDebug>

namespace {
    qreal dpr(qreal value) {
    #ifdef Q_OS_MAC
        // On mac the DPI is always 72 so we should not scale it
        return value;
    #else
        static const qreal scale = qreal(qt_defaultDpiX()) / 96.0;
        return value * scale;
    #endif
    }

    int dp(qreal value) {
        return dpr(value);
    }

//    bool isDark(const QColor &color) {
//        return color.toHsl().lightness() < 128;
//    }

    const int kMenuItemHPadding = dp(8);
    const int kMenuItemVPadding = dp(6);
    const int kArrowSize = dp(8);
    const int kMenuCheckSize = dp(16);
    const int kMinimumButtonWidth = dp(80);
    const int kButtonHeight = dp(28);

    enum Direction {
        Up,
        Down,
        Left,
        Right
    };

    class RectCoordinates {
        QRectF rect;
    public:
        RectCoordinates(const QRectF &rect) : rect(rect) {}

        qreal x(qreal x) {
            if (rect.isEmpty())
                return 0;
            return rect.left() + rect.width() * x;
        }

        qreal y(qreal y) {
            if (rect.isEmpty())
                return 0;
            return rect.top() + rect.height() * y;
        }
    };

    void drawArrow(Direction direction, QPainter *painter, const QRectF &rect, const QColor &color) {
        QPainterPath path;

        QRectF arrowRect(QPointF(), QSizeF(kArrowSize, kArrowSize));
        arrowRect.moveTo(rect.left() + (rect.width() - kArrowSize) / 2, rect.top() + (rect.height() - kArrowSize) / 2);

        RectCoordinates rc(arrowRect);

        switch (direction) {
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
        default:
            break;
        }

        QPen pen(color, dpr(1.2));
        pen.setJoinStyle(Qt::MiterJoin);
        pen.setCapStyle(Qt::FlatCap);

        painter->save();

        painter->setPen(pen);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawPath(path);

        painter->restore();
    }

    void drawMenuCheckMark(QPainter *painter, const QRect &rect, const QColor &color) {
        QRect checkRect(QPoint(), QSize(kMenuCheckSize, kMenuCheckSize));
        checkRect.moveTo(rect.left() + (rect.width() - kMenuCheckSize) / 2, rect.top() + (rect.height() - kMenuCheckSize) / 2);

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
}

QnNxStylePrivate::QnNxStylePrivate() :
    QCommonStylePrivate()
{
}

QnNxStyle::QnNxStyle() {}

void QnNxStyle::setGenericPalette(const QnGenericPalette &palette)
{
    m_palette = palette;
}

QnPaletteColor QnNxStyle::findColor(const QColor &color) const
{
    return m_palette.color(color);
}

void QnNxStyle::drawPrimitive(
        PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    switch (element)
    {
    case PE_FrameFocusRect:
        return;
    case PE_PanelButtonTool:
    case PE_PanelButtonCommand: {
        painter->save();

        const bool pressed = option->state & State_Sunken;
        const bool hovered = option->state & State_MouseOver;

        QnPaletteColor mainColor = findColor(option->palette.color(QPalette::Button));

        QColor buttonColor = mainColor;
        QColor shadowColor = mainColor.darker(2);

        if (pressed)
        {
            buttonColor = mainColor.darker(1);
        }
        else if (hovered)
        {
            buttonColor = mainColor.lighter(1);
            shadowColor = mainColor.darker(1);
        }

        painter->setPen(Qt::NoPen);
        painter->setBrush(buttonColor);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawRoundedRect(option->rect.adjusted(0, 0, 0, -1), 2, 2);
        painter->setRenderHint(QPainter::Antialiasing, false);

        QPen shadowPen(shadowColor);
        painter->setPen(shadowPen);

        if (pressed)
        {
            painter->drawLine(option->rect.left() + 1, option->rect.top() + 0.5,
                              option->rect.right() - 1, option->rect.top() + 0.5);
        }
        else
        {
            painter->drawLine(option->rect.left() + 1, option->rect.bottom(),
                              option->rect.right() - 1, option->rect.bottom());
        }

        painter->restore();
        return;
    }
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
            return;
        }
    case PE_FrameGroupBox:
        return;
    case PE_IndicatorCheckBox:
        {
            painter->save();

            QPen pen(option->palette.color(QPalette::WindowText));
            pen.setJoinStyle(Qt::MiterJoin);
            pen.setCapStyle(Qt::FlatCap);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);

            QRectF rect = option->rect.adjusted(2, 2, -3, -3);

            if (option->state.testFlag(State_On))
            {
                QPainterPath path;
                path.moveTo(rect.right() - rect.width() * 0.3, rect.top());
                path.lineTo(rect.left(), rect.top());
                path.lineTo(rect.left(), rect.bottom());
                path.lineTo(rect.right(), rect.bottom());
                path.lineTo(rect.right(), rect.top() + rect.height() * 0.6);

                painter->setPen(pen);
                painter->drawPath(path);

                path = QPainterPath();
                path.moveTo(rect.left() + rect.width() * 0.2, rect.top() + rect.height() * 0.45);
                path.lineTo(rect.left() + rect.width() * 0.5, rect.top() + rect.height() * 0.75);
                path.lineTo(rect.right() + rect.width() * 0.05, rect.top() + rect.height() * 0.15);

                pen.setWidthF(dp(2));
                painter->setPen(pen);
                painter->setRenderHint(QPainter::Antialiasing);
                painter->drawPath(path);
            }
            else
            {
                painter->drawRect(rect);

                if (option->state.testFlag(State_NoChange))
                {
                    pen.setWidth(2);
                    painter->setPen(pen);
                    painter->drawLine(QPointF(rect.left() + 2, rect.top() + rect.height() / 2.0),
                                      QPointF(rect.right() - 1, rect.top() + rect.height() / 2.0));
                }
            }

            painter->restore();
            return;
        }
    case PE_IndicatorRadioButton:
        {
            painter->save();

            QRect rect = option->rect.adjusted(1, 1, -2, -2);

            QPen pen(option->palette.color(QPalette::WindowText), 1.2);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->setRenderHint(QPainter::Antialiasing);
            painter->drawEllipse(rect);

            if (option->state.testFlag(State_On))
            {
                painter->setBrush(option->palette.windowText());
                painter->setPen(Qt::NoPen);
                painter->drawEllipse(rect.adjusted(3, 3, -3, -3));
            }
            else if (option->state.testFlag(State_NoChange))
            {
                pen.setWidth(2);
                painter->setPen(pen);
                painter->drawLine(QPointF(rect.left() + 4, rect.top() + rect.height() / 2.0),
                                  QPointF(rect.right() - 3, rect.top() + rect.height() / 2.0));
            }

            painter->restore();
            return;
        }
    case PE_FrameTabBarBase: {
        if (const QStyleOptionTabBarBase *tabBar = qstyleoption_cast<const QStyleOptionTabBarBase*>(option)) {
            QRect rect = tabBar->tabBarRect;
            rect.setLeft(tabBar->rect.left());
            rect.setRight(tabBar->rect.right());
            painter->fillRect(rect, option->palette.window());

            painter->save();
            painter->setPen(tabBar->palette.color(QPalette::Dark));
            painter->drawLine(rect.topLeft(), rect.topRight());
            painter->restore();
            return;
        }
        break;
    }
    case PE_FrameTabWidget:
        return;
    case PE_PanelMenu: {
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(option->palette.window());
        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawRoundedRect(option->rect, dp(2), dp(2));
        painter->restore();
        return;
    }
    case PE_FrameMenu:
        return;
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
        if (const QStyleOptionComboBox *comboBox = qstyleoption_cast<const QStyleOptionComboBox *>(option))
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

            if (comboBox->subControls & SC_ComboBoxArrow)
            {
                QRectF rect = subControlRect(CC_ComboBox, comboBox, SC_ComboBoxArrow, widget);
                drawArrow(Down, painter, rect.translated(0, -1), option->palette.color(QPalette::Text));
            }

            painter->restore();
            return;
        }
        break;
    case CC_Slider:
        if (const QStyleOptionSlider *slider = qstyleoption_cast<const QStyleOptionSlider*>(option)) {
            QRect groove = proxy()->subControlRect(CC_Slider, option, SC_SliderGroove, widget);
            QRect handle = proxy()->subControlRect(CC_Slider, option, SC_SliderHandle, widget);

            painter->save();

            if (slider->subControls.testFlag(SC_SliderGroove)) {
                painter->setPen(QPen(slider->palette.mid(), 1));
                painter->setBrush(slider->palette.light());
                if (slider->orientation == Qt::Horizontal) {
                    if (slider->upsideDown) {
                        int mid = sliderPositionFromValue(slider->minimum, slider->maximum, slider->sliderPosition, slider->rect.width());
                        mid = groove.width() - mid;
                        painter->drawRect(mid, groove.y(), groove.width() - mid, groove.height());
                        painter->setBrush(slider->palette.mid());
                        painter->drawRect(groove.x(), groove.y(), mid, groove.height());
                    } else {
                        int mid = sliderPositionFromValue(slider->minimum, slider->maximum, slider->sliderPosition, slider->rect.width());
                        painter->drawRect(groove.x(), groove.y(), mid, groove.height());
                        painter->setBrush(slider->palette.mid());
                        painter->drawRect(mid, groove.y(), groove.width() - mid, groove.height());
                    }
                } else {
                    if (slider->upsideDown) {
                        int mid = sliderPositionFromValue(slider->minimum, slider->maximum, slider->sliderPosition, slider->rect.height());
                        mid = groove.height() - mid;
                        painter->drawRect(groove.x(), mid, groove.width(), groove.height() - mid);
                        painter->setBrush(slider->palette.mid());
                        painter->drawRect(groove.x(), groove.y(), groove.width(), mid);
                    } else {
                        int mid = sliderPositionFromValue(slider->minimum, slider->maximum, slider->sliderPosition, slider->rect.height());
                        painter->drawRect(groove.x(), groove.y(), groove.width(), mid);
                        painter->setBrush(slider->palette.mid());
                        painter->drawRect(groove.x(), mid, groove.width(), groove.height() - mid);
                    }
                }
            }

            if (slider->subControls.testFlag(SC_SliderTickmarks)) {
                bool horizontal = slider->orientation == Qt::Horizontal;
                bool ticksAbove = slider->tickPosition & QSlider::TicksAbove;
                bool ticksBelow = slider->tickPosition & QSlider::TicksBelow;

                painter->setPen(slider->palette.color(QPalette::Midlight));
                int tickSize = proxy()->pixelMetric(PM_SliderTickmarkOffset, option, widget);
                int available = proxy()->pixelMetric(PM_SliderSpaceAvailable, slider, widget);
                int interval = slider->tickInterval;
                if (interval <= 0) {
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
                while (v <= slider->maximum + 1) {
                    if (v == slider->maximum + 1 && interval == 1)
                        break;
                    const int v_ = qMin(v, slider->maximum);
                    int pos = sliderPositionFromValue(slider->minimum, slider->maximum,
                                                      v_, (horizontal
                                                           ? slider->rect.width()
                                                           : slider->rect.height()) - len,
                                                      slider->upsideDown) + len / 2;
                    const int extra = 2;

                    if (horizontal) {
                        if (ticksAbove) {
                            painter->drawLine(pos, slider->rect.top() + extra,
                                              pos, slider->rect.top() + tickSize);
                        }
                        if (ticksBelow) {
                            painter->drawLine(pos, slider->rect.bottom() - extra,
                                              pos, slider->rect.bottom() - tickSize);
                        }
                    } else {
                        if (ticksAbove) {
                            painter->drawLine(slider->rect.left() + extra, pos,
                                              slider->rect.left() + tickSize, pos);
                        }
                        if (ticksBelow) {
                            painter->drawLine(slider->rect.right() - extra, pos,
                                              slider->rect.right() - tickSize, pos);
                        }
                    }
                    // in the case where maximum is max int
                    int nextInterval = v + interval;
                    if (nextInterval < v)
                        break;

                    v = nextInterval;
                }
            }

            if (slider->subControls.testFlag(SC_SliderHandle)) {
                QPen pen(slider->palette.light(), 2);
                painter->setPen(pen);
                if (option->state.testFlag(State_Sunken))
                    painter->setBrush(pen.brush());
                else
                    painter->setBrush(slider->palette.dark());

                painter->setRenderHint(QPainter::Antialiasing);
                painter->drawEllipse(handle.adjusted(1, 1, -1, -1));
                painter->setRenderHint(QPainter::Antialiasing, false);
            }

            painter->restore();
            return;
        }
        break;
    case CC_GroupBox:
        if (const QStyleOptionGroupBox *groupBox = qstyleoption_cast<const QStyleOptionGroupBox*>(option))
        {
            painter->save();

            const bool flat = groupBox->features.testFlag(QStyleOptionFrame::Flat);
            QStyleOptionGroupBox opt = *groupBox;

            if (flat)
            {
                QFont font = painter->font();
                font.setPixelSize(font.pixelSize() + 2);
                font.setWeight(QFont::DemiBold);
                painter->setFont(font);
                opt.fontMetrics = QFontMetrics(font);
            }

            QRect labelRect = subControlRect(CC_GroupBox, &opt, SC_GroupBoxLabel, widget);

            if (groupBox->subControls & SC_GroupBoxFrame)
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
                    painter->setPen(mainColor);
                    QPainterPath path;
                    path.moveTo(labelRect.left() - dp(8), rect.top());
                    path.lineTo(rect.topLeft());
                    path.lineTo(rect.bottomLeft());
                    path.lineTo(rect.bottomRight());
                    path.lineTo(rect.topRight());
                    path.lineTo(labelRect.right() + dp(8), rect.top());
                    painter->drawPath(path);
                }
            }

            if (groupBox->subControls & SC_GroupBoxLabel)
            {
                QnPaletteColor mainColor = findColor(option->palette.color(QPalette::WindowText));
                painter->setPen(flat ? mainColor : mainColor.darker(12));
                drawItemText(painter, labelRect, Qt::AlignCenter,
                             groupBox->palette, groupBox->state & QStyle::State_Enabled,
                             groupBox->text);
            }

            painter->restore();

            return;
        }
        break;
    case CC_SpinBox:
        if (const QStyleOptionSpinBox *spinBox = qstyleoption_cast<const QStyleOptionSpinBox*>(option))
        {
            if (option->subControls & SC_SpinBoxFrame)
                proxy()->drawPrimitive(PE_PanelLineEdit, option, painter, widget);

            if (option->subControls & SC_SpinBoxUp && option->subControls & SC_SpinBoxDown)
            {
                drawArrow(Up, painter, subControlRect(control, option, SC_SpinBoxUp, widget)
                          .translated(0, dp(2)), option->palette.color(QPalette::Text));
                drawArrow(Down, painter, subControlRect(control, option, SC_SpinBoxDown, widget)
                          .translated(0, -dp(2)), option->palette.color(QPalette::Text));
            }
            return;
        }
        break;
    case CC_ScrollBar:
        if (const QStyleOptionSlider *scrollBar = qstyleoption_cast<const QStyleOptionSlider*>(option))
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
    default:
        break;
    }

    base_type::drawComplexControl(control, option, painter, widget);
}

void QnNxStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    switch (element)
    {
    case CE_ShapedFrame:
        if (const QStyleOptionFrame *frame = qstyleoption_cast<const QStyleOptionFrame *>(option))
        {
            switch (frame->frameShape)
            {
            case QFrame::HLine:
                {
                    QnPaletteColor mainColor = findColor(option->palette.color(QPalette::Dark)).darker(1);

                    QColor topColor = mainColor.darker(2);
                    QColor bottomColor;

                    if (frame->state.testFlag(State_Sunken))
                    {
                        bottomColor = mainColor;
                    }
                    else if (frame->state.testFlag(State_Raised))
                    {
                        bottomColor = topColor;
                        topColor = mainColor;
                    }

                    painter->save();

                    painter->setPen(topColor);
                    painter->drawLine(frame->rect.topLeft(), frame->rect.topRight());

                    if (bottomColor.isValid())
                    {
                        painter->setPen(bottomColor);
                        painter->drawLine(frame->rect.left(), frame->rect.top() + 1,
                                          frame->rect.right(), frame->rect.top() + 1);
                    }

                    painter->restore();
                    return;
                }
            default:
                break;
            }
        }
        break;
    case CE_ComboBoxLabel:
        if (const QStyleOptionComboBox *comboBox = qstyleoption_cast<const QStyleOptionComboBox*>(option)) {
            QStyleOptionComboBox opt = *comboBox;
            opt.rect.setLeft(opt.rect.left() + dp(6));
            base_type::drawControl(element, &opt, painter, widget);
            return;
        }
        break;
    case CE_CheckBox:
    case CE_RadioButton:
        if (const QStyleOptionButton *button = qstyleoption_cast<const QStyleOptionButton*>(option))
        {
            QStyleOptionButton opt = *button;

            QnPaletteColor mainColor = findColor(button->palette.color(QPalette::Text));
            QColor color = mainColor.darker(6);

            if (opt.state & State_Off)
            {
                if (opt.state & State_MouseOver)
                    color = mainColor.darker(4);
            }
            else if (opt.state & State_On || opt.state & State_NoChange)
            {
                if (element != CE_RadioButton && opt.state & State_MouseOver)
                    color = mainColor.lighter(3);
                else
                    color = mainColor;
            }

            opt.palette.setColor(QPalette::WindowText, color);

            base_type::drawControl(element, &opt, painter, widget);
            return;
        }
        break;
    case CE_TabBarTabShape:
        if (const QStyleOptionTab *tab = qstyleoption_cast<const QStyleOptionTab*>(option))
        {
            switch (tab->shape)
            {
            case QTabBar::TriangularNorth:
                if (!(tab->state & QStyle::State_Selected) && (tab->state & QStyle::State_MouseOver))
                {
                    painter->save();

                    painter->fillRect(tab->rect, tab->palette.mid());

                    painter->setPen(tab->palette.color(QPalette::Window));
                    painter->drawLine(tab->rect.topLeft(), tab->rect.topRight());

                    painter->restore();
                }

                return;
            default:
                break;
            }
        }
        break;
    case CE_TabBarTabLabel:
        if (const QStyleOptionTab *tab = qstyleoption_cast<const QStyleOptionTab*>(option))
        {
            painter->save();

            if (tab->shape == QTabBar::TriangularNorth)
            {
                QFont font = painter->font();
                font.setPixelSize(font.pixelSize() - 1);
                painter->setFont(font);
            }

            int textFlags = Qt::AlignCenter | Qt::TextHideMnemonic;
            QPalette::ColorRole colorRole = QPalette::WindowText;

            if (tab->state & QStyle::State_Selected)
            {
                colorRole = QPalette::Highlight;

                if (tab->shape == QTabBar::TriangularNorth)
                {
                    QFontMetrics fm(painter->font());
                    QRect rect = fm.boundingRect(tab->rect, textFlags, tab->text);

                    rect.setTop(tab->rect.bottom() - 2);
                    rect.setBottom(tab->rect.bottom() - 1);
                    painter->fillRect(rect, tab->palette.brush(colorRole));
                }
            }
            else if (tab->state & State_MouseOver)
            {
                colorRole = QPalette::Light;
            }

            drawItemText(painter, tab->rect, Qt::AlignCenter | Qt::TextHideMnemonic,
                         tab->palette, tab->state & QStyle::State_Enabled, tab->text,
                         colorRole);

            painter->restore();
            return;
        }
    case CE_HeaderSection:
        if (const QStyleOptionHeader *header = qstyleoption_cast<const QStyleOptionHeader*>(option))
        {
            painter->fillRect(header->rect, header->palette.window());

            if (header->orientation == Qt::Horizontal)
            {
                painter->save();
                painter->setPen(header->palette.color(QPalette::Dark));
                painter->drawLine(header->rect.bottomLeft(), header->rect.bottomRight());
                painter->restore();
            }
            return;
        }
        break;
    case CE_HeaderEmptyArea:
        if (const QStyleOptionHeader *header = qstyleoption_cast<const QStyleOptionHeader*>(option)) {
            painter->fillRect(header->rect, header->palette.window());
            return;
        }
        break;
    case CE_MenuItem:
        if (const QStyleOptionMenuItem *menuItem = qstyleoption_cast<const QStyleOptionMenuItem*>(option)) {
            if (menuItem->menuItemType == QStyleOptionMenuItem::Separator) {
                painter->save();
                QColor color = menuItem->palette.color(QPalette::Mid);
                color.setAlpha(100);
                painter->setPen(color);
                int y = menuItem->rect.top() + menuItem->rect.height() / 2;
                painter->drawLine(kMenuItemHPadding, y, menuItem->rect.right() - kMenuItemHPadding, y);
                painter->restore();
                break;
            }

            painter->save();

            bool enabled = menuItem->state & State_Enabled;
            bool selected = enabled && (menuItem->state & State_Selected);

            if (selected) {
                painter->fillRect(menuItem->rect, menuItem->palette.highlight());
            }

            int xPos = kMenuItemHPadding + dp(16);
            int y = menuItem->rect.y();

            int textFlags = Qt::AlignVCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
            if (!styleHint(SH_UnderlineShortcut, menuItem, widget, nullptr))
                textFlags |= Qt::TextHideMnemonic;

            QRect textRect(xPos, y + kMenuItemVPadding, menuItem->rect.width() - xPos - kMenuItemHPadding, menuItem->rect.height() - 2 * kMenuItemVPadding);

            if (!menuItem->text.isEmpty()) {
                QString text = menuItem->text;
                QString shortcut;
                int tabPosition = text.indexOf(QLatin1Char('\t'));
                if (tabPosition >= 0) {
                    shortcut = text.mid(tabPosition + 1);
                    text = text.left(tabPosition);

                    painter->setPen(menuItem->palette.color(QPalette::Mid));
                    painter->drawText(textRect, textFlags | Qt::AlignRight, shortcut);
                }

                painter->setPen(menuItem->palette.color(QPalette::WindowText));
                painter->drawText(textRect, textFlags | Qt::AlignLeft, text);
            }

            if (menuItem->checked) {
                drawMenuCheckMark(painter, QRect(kMenuItemHPadding, menuItem->rect.y(), dp(16), menuItem->rect.height()),
                                  menuItem->palette.color(selected ? QPalette::HighlightedText : QPalette::WindowText));
            }

            if (menuItem->menuItemType == QStyleOptionMenuItem::SubMenu) {
                drawArrow(Right, painter,
                          QRect(menuItem->rect.right() - kMenuItemVPadding - kArrowSize, menuItem->rect.top(), kArrowSize, menuItem->rect.height()),
                          menuItem->palette.color(selected ? QPalette::HighlightedText : QPalette::WindowText));
            }

            painter->restore();
            return;
        }
        break;
    default:
        break;
    }

    base_type::drawControl(element, option, painter, widget);
}

QRect QnNxStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const {
    QRect rect = base_type::subControlRect(control, option, subControl, widget);

    switch (control) {
    case CC_Slider:
        if (const QStyleOptionSlider *slider = qstyleoption_cast<const QStyleOptionSlider*>(option)) {
            int tickSize = proxy()->pixelMetric(PM_SliderTickmarkOffset, option, widget);
            switch (subControl) {
            case SC_SliderHandle: {
                if (slider->orientation == Qt::Horizontal) {
                    rect.setHeight(proxy()->pixelMetric(PM_SliderControlThickness));
                    rect.setWidth(proxy()->pixelMetric(PM_SliderLength));
                    int centerY = slider->rect.center().y() - rect.height() / 2;
                    if (slider->tickPosition & QSlider::TicksAbove)
                        centerY += tickSize;
                    if (slider->tickPosition & QSlider::TicksBelow)
                        centerY -= tickSize;
                    rect.moveTop(centerY);
                } else {
                    rect.setWidth(proxy()->pixelMetric(PM_SliderControlThickness));
                    rect.setHeight(proxy()->pixelMetric(PM_SliderLength));
                    int centerX = slider->rect.center().x() - rect.width() / 2;
                    if (slider->tickPosition & QSlider::TicksAbove)
                        centerX += tickSize;
                    if (slider->tickPosition & QSlider::TicksBelow)
                        centerX -= tickSize;
                    rect.moveLeft(centerX);
                }
                break;
            }
            case SC_SliderGroove: {
                QPoint grooveCenter = slider->rect.center();
                const int grooveThickness = dp(3);
                if (slider->orientation == Qt::Horizontal) {
                    rect.setHeight(grooveThickness);
                    if (slider->tickPosition & QSlider::TicksAbove)
                        grooveCenter.ry() += tickSize;
                    if (slider->tickPosition & QSlider::TicksBelow)
                        grooveCenter.ry() -= tickSize;
                } else {
                    rect.setWidth(grooveThickness);
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
    case CC_ComboBox:
        if (const QStyleOptionComboBox *comboBox = qstyleoption_cast<const QStyleOptionComboBox*>(option)) {
            switch (subControl) {
            case SC_ComboBoxArrow: {
                rect = QRect(comboBox->rect.right() - comboBox->rect.height(), 0,
                             comboBox->rect.height(), comboBox->rect.height());
                rect.adjust(0, 1, 0, -1);
                break;
            }
            case SC_ComboBoxEditField: {
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
    case CC_SpinBox:
        if (const QStyleOptionSpinBox *spinBox = qstyleoption_cast<const QStyleOptionSpinBox*>(option)) {
            switch (subControl) {
            case SC_SpinBoxEditField: {
                int frameWidth = pixelMetric(PM_SpinBoxFrameWidth, option, widget);
                int buttonWidth = subControlRect(control, option, SC_SpinBoxDown, widget).width();
                rect = spinBox->rect;
                rect.setRight(rect.right() - buttonWidth);
                rect.adjust(frameWidth, frameWidth, 0, -frameWidth);
                break;
            }
            default:
                break;
            }
        }
        break;
    case CC_GroupBox:
        if (const QStyleOptionGroupBox *groupBox = qstyleoption_cast<const QStyleOptionGroupBox*>(option)) {
            switch (subControl) {
            case SC_GroupBoxFrame:
                if (groupBox->features & QStyleOptionFrame::Flat) {
                    QRect labelRect = subControlRect(CC_GroupBox, option, SC_GroupBoxLabel, widget);
                    rect.setTop(labelRect.bottom() + dp(8));
                }
                break;
            case SC_GroupBoxLabel:
                if (!(groupBox->features & QStyleOptionFrame::Flat)) {
                    rect.moveLeft(dp(16));
                }
                break;
            case SC_GroupBoxContents: {
                if (groupBox->features & QStyleOptionFrame::Flat) {
                    QRect labelRect = subControlRect(CC_GroupBox, option, SC_GroupBoxLabel, widget);
                    rect.setTop(labelRect.bottom() + dp(24));
                }
                break;
            }
            default:
                break;
            }
        }
        break;
    case CC_ScrollBar:
        if (const QStyleOptionSlider *scrollBar = qstyleoption_cast<const QStyleOptionSlider*>(option))
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
                rect = widget->rect();
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

QRect QnNxStyle::subElementRect(QStyle::SubElement subElement, const QStyleOption *option, const QWidget *widget) const {
    QRect rect = base_type::subElementRect(subElement, option, widget);

    switch (subElement) {
    case SE_LineEditContents:
        rect.setLeft(rect.left() + dp(6));
        break;
    case SE_PushButtonLayoutItem:
        if (qobject_cast<const QDialogButtonBox *>(widget))
        {
            const int shift = 8;
            rect = option->rect.adjusted(-shift, -shift, shift, shift);
        }
        break;
    default:
        break;
    }

    return rect;
}

int QnNxStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const {
    switch (metric) {
    case PM_ButtonMargin:
        return dp(10);
    case PM_ButtonShiftVertical:
    case PM_ButtonShiftHorizontal:
    case PM_TabBarTabShiftVertical:
    case PM_TabBarTabShiftHorizontal:
        return 0;
    case PM_DefaultFrameWidth:
        return 1;
    case PM_SliderThickness:
        return dp(18);
    case PM_SliderControlThickness:
    case PM_SliderLength:
        return dp(16);
    case PM_IndicatorWidth:
    case PM_IndicatorHeight:
        return dp(16);
    case PM_ExclusiveIndicatorWidth:
    case PM_ExclusiveIndicatorHeight:
        return dp(16);
    case PM_TabBarTabHSpace:
        return 9;
    case PM_TabBarBaseOverlap:
        return 2;
    case PM_TabBarBaseHeight:
        return 36;
    case PM_ScrollBarExtent:
        return 8;
    case PM_ScrollBarSliderMin:
        return 8;
    default:
        break;
    }

    return base_type::pixelMetric(metric, option, widget);
}

QSize QnNxStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size, const QWidget *widget) const {
    switch (type) {
    case CT_PushButton:
        return QSize(qMax(kMinimumButtonWidth, size.width() + 2 * pixelMetric(PM_ButtonMargin, option, widget)), qMax(size.height(), kButtonHeight));
    case CT_LineEdit:
        return QSize(size.width(), qMax(size.height(), kButtonHeight));
    case CT_ComboBox:
        return QSize(qMax(kMinimumButtonWidth, size.width() + 2 * pixelMetric(PM_ButtonMargin, option, widget)), qMax(size.height(), kButtonHeight));
    case CT_TabBarTab:
        {
            int height = qMax(size.height(), pixelMetric(PM_TabBarBaseHeight, option, widget));
            if (qobject_cast<const QTabBar*>(widget))
                height = qMin(height, widget->maximumHeight());
            return QSize(size.width(), height);
        }
    case CT_MenuItem:
        return QSize(size.width() + dp(24) + 2 * kMenuItemHPadding, size.height() + 2 * kMenuItemVPadding);
    default:
        break;
    }

    return base_type::sizeFromContents(type, option, size, widget);
}

int QnNxStyle::styleHint(StyleHint sh, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *shret) const {
    switch (sh) {
    case SH_GroupBox_TextLabelColor:
        if (const QStyleOptionGroupBox *groupBox = qstyleoption_cast<const QStyleOptionGroupBox*>(option)) {
            if (groupBox->features & QStyleOptionFrame::Flat)
                return (int)groupBox->palette.color(QPalette::Text).rgba();
            else
                return (int)groupBox->palette.color(QPalette::WindowText).rgba();
        }
        break;
    case SH_Menu_MouseTracking:
    case SH_ComboBox_ListMouseTracking:
        return 1;
    case SH_ComboBox_PopupFrameStyle:
        return QFrame::NoFrame;
    case QStyle::SH_DialogButtonBox_ButtonsHaveIcons:
        return 0;
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
        QFont font = widget->font();
        font.setWeight(QFont::DemiBold);
        widget->setFont(font);
        widget->setAttribute(Qt::WA_Hover);
    }

    if (qobject_cast<QLineEdit*>(widget) ||
        qobject_cast<QComboBox*>(widget) ||
        qobject_cast<QSpinBox*>(widget) ||
        qobject_cast<QCheckBox*>(widget) ||
        qobject_cast<QRadioButton*>(widget) ||
        qobject_cast<QTabBar*>(widget) ||
        qobject_cast<QScrollBar*>(widget))
    {
        widget->setAttribute(Qt::WA_Hover);
    }

    if (qobject_cast<QMenu*>(widget))
    {
        QPalette palette = widget->palette();
        palette.setColor(QPalette::Window, QColor("#e1e7ea"));
        palette.setColor(QPalette::WindowText, QColor("#121517"));
        palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#80121517"));
        palette.setColor(QPalette::Mid, QColor("#698796"));
        palette.setColor(QPalette::Disabled, QPalette::Mid, QColor("#80698796"));
        palette.setColor(QPalette::Highlight, QColor("#e1e7ea").darker(120));
        palette.setColor(QPalette::HighlightedText, QColor("#121517"));
        widget->setPalette(palette);
        widget->setAttribute(Qt::WA_TranslucentBackground);
#ifdef Q_OS_WIN
        widget->setWindowFlags(widget->windowFlags() | Qt::FramelessWindowHint);
#endif
    }
}

void QnNxStyle::unpolish(QWidget *widget) {
    if (qobject_cast<QAbstractButton*>(widget)
        || qobject_cast<QTabBar*>(widget))
    {
        widget->setFont(qApp->font());
    }

    if (qobject_cast<QAbstractItemView*>(widget)
        || qobject_cast<QMenu*>(widget))
    {
        widget->setPalette(qApp->palette());
    }

    base_type::unpolish(widget);
}
