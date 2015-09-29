#include "nx_style.h"
#include "nx_style_p.h"

#include <QPainter>
#include <QStyleOption>
#include <QStyleOptionButton>
#include <QAbstractButton>
#include <QLineEdit>
#include <QMenu>
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

    const int menuItemHPadding = dp(8);
    const int menuItemVPadding = dp(6);
    const int arrowSize = dp(8);
    const int menuCheckSize = dp(16);
    const int minimumButtonWidth = dp(80);
    const int buttonHeight = dp(30);

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

        QRectF arrowRect(QPointF(), QSizeF(arrowSize, arrowSize));
        arrowRect.moveTo(rect.left() + (rect.width() - arrowSize) / 2, rect.top() + (rect.height() - arrowSize) / 2);

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
        QRect checkRect(QPoint(), QSize(menuCheckSize, menuCheckSize));
        checkRect.moveTo(rect.left() + (rect.width() - menuCheckSize) / 2, rect.top() + (rect.height() - menuCheckSize) / 2);

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

void QnNxStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    switch (element) {
    case PE_FrameFocusRect:
        return;
    case PE_PanelButtonTool:
    case PE_PanelButtonCommand: {
        painter->save();

        QColor panelColor = option->palette.color(option->palette.currentColorGroup(), QPalette::Button);;
        QColor windowColor = option->palette.color(QPalette::Window);

        if (option->state.testFlag(State_Sunken))
            panelColor = panelColor.darker(110);
        else if (const QStyleOptionButton *button = qstyleoption_cast<const QStyleOptionButton*>(option)) {
            if (button->features.testFlag(QStyleOptionButton::Flat))
                return;
        }

        painter->setPen(Qt::NoPen);
        painter->setBrush(panelColor);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawRoundedRect(option->rect.adjusted(0, 0, 0, -1), 2, 2);
        painter->setRenderHint(QPainter::Antialiasing, false);
        if (option->state.testFlag(State_Raised)) {
            QPen pen(windowColor.darker(110));
            painter->setPen(pen);
            painter->drawLine(option->rect.left() + 2, option->rect.bottom() + 0.5, option->rect.right() - 2, option->rect.bottom() + 0.5);
        }

        painter->restore();
        return;
    }
    case PE_FrameLineEdit: {
        painter->save();

        QPen pen(option->palette.color(QPalette::Shadow));
        painter->setPen(pen);
        painter->setBrush(option->palette.base());

        painter->drawRect(QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5));

        painter->restore();
        return;
    }
    case PE_FrameGroupBox:
        if (const QStyleOptionFrame *frame = qstyleoption_cast<const QStyleOptionFrame*>(option)) {
            painter->save();

            QPen pen(frame->palette.midlight(), dp(1));
            painter->setPen(pen);

            QRectF rect = frame->rect.adjusted(pen.widthF() / 2, pen.widthF() / 2, -1 - pen.widthF() / 2, -1 - pen.widthF() / 2);

            painter->drawLine(rect.topLeft(), rect.topRight());

            painter->restore();
            return;
        }
        break;
    case PE_IndicatorCheckBox: {
        painter->save();

        QPen pen(option->palette.light(), dp(1));
        pen.setJoinStyle(Qt::MiterJoin);
        pen.setCapStyle(Qt::FlatCap);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);

        QRectF rect = option->rect.adjusted(1, 1, -pen.widthF() - 2, -pen.widthF() - 2);

        if (option->state.testFlag(State_On)) {
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
        } else {
            painter->drawRect(rect);
        }

        painter->restore();
        return;
    }
    case PE_IndicatorRadioButton: {
        painter->save();

        QPen pen(option->palette.light(), dpr(1.5));
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawEllipse(option->rect.adjusted(1, 2, -1, 0));

        if (option->state.testFlag(State_On)) {
            painter->setBrush(option->palette.light());
            painter->setPen(Qt::NoPen);
            int markSize = option->rect.width() * 0.5;
            QRectF rect(option->rect.left() + (option->rect.width() - markSize) / 2,
                        option->rect.top() + (option->rect.height() - markSize) / 2 + 1,
                        markSize, markSize);
            painter->drawEllipse(rect);
        }

        painter->restore();
        return;
    }
    case PE_FrameTabBarBase: {
        if (const QStyleOptionTabBarBase *tabBar = qstyleoption_cast<const QStyleOptionTabBarBase*>(option)) {
            painter->save();

            QRect rect = tabBar->tabBarRect;
            rect.setLeft(tabBar->rect.left());
            rect.setRight(tabBar->rect.right());

            QPen pen(tabBar->palette.shadow(), dp(1));
            painter->setPen(pen);

            painter->fillRect(rect, option->palette.button());
            painter->drawLine(rect.bottomLeft(), rect.bottomRight());

            painter->restore();
        }
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

void QnNxStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const {
    switch (control) {
    case CC_ComboBox:
        if (const QStyleOptionComboBox *comboBox = qstyleoption_cast<const QStyleOptionComboBox*>(option)) {
            bool hasFocus = comboBox->state & State_HasFocus && comboBox->state & State_KeyboardFocusChange;
            bool sunken = comboBox->state & State_On;
            bool isEnabled = comboBox->state & State_Enabled;

            painter->save();

            QRect buttonRect = comboBox->rect;

            if (comboBox->editable) {
                proxy()->drawPrimitive(PE_FrameLineEdit, comboBox, painter, widget);
            } else {
                QStyleOptionButton buttonOption;
                buttonOption.QStyleOption::operator=(*comboBox);
                buttonOption.rect = buttonRect;
                buttonOption.state = comboBox->state & (State_Enabled | State_MouseOver | State_HasFocus | State_KeyboardFocusChange);
                if (sunken) {
                    buttonOption.state |= State_Sunken;
                    buttonOption.state &= ~State_MouseOver;
                }
                proxy()->drawPrimitive(PE_PanelButtonCommand, &buttonOption, painter, widget);
            }

            if (comboBox->subControls & SC_ComboBoxArrow) {
                QRectF rect = subControlRect(CC_ComboBox, comboBox, SC_ComboBoxArrow, widget);
                drawArrow(Down, painter, rect.translated(0, -dp(2)), option->palette.color(QPalette::Text));
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
        if (const QStyleOptionGroupBox *groupBox = qstyleoption_cast<const QStyleOptionGroupBox*>(option)) {
            painter->save();

            QRect labelRect = subControlRect(CC_GroupBox, groupBox, SC_GroupBoxLabel, widget);

            QPen pen(groupBox->palette.midlight(), dp(1));
            painter->setPen(pen);

            if (groupBox->subControls & SC_GroupBoxFrame) {
                QRect rect = subControlRect(CC_GroupBox, groupBox, SC_GroupBoxFrame, widget).adjusted(pen.widthF() / 2, pen.widthF() / 2, -1 - pen.widthF() / 2, -1 - pen.widthF() / 2);

                if (groupBox->features & QStyleOptionFrame::Flat) {
                    painter->drawLine(rect.topLeft(), rect.topRight());
                } else {
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

            painter->restore();

            if (groupBox->subControls & SC_GroupBoxLabel) {
                QPalette::ColorRole textRole = groupBox->features & QStyleOptionFrame::Flat ? QPalette::Text : QPalette::WindowText;
                drawItemText(painter, labelRect, Qt::AlignCenter, groupBox->palette, groupBox->state & QStyle::State_Enabled, groupBox->text, textRole);
            }

            return;
        }
        break;
    case CC_SpinBox:
        if (const QStyleOptionSpinBox *spinBox = qstyleoption_cast<const QStyleOptionSpinBox*>(option)) {
            if (option->subControls & SC_SpinBoxFrame) {
                if (!spinBox->frame) {
                    // TODO
                } else {
                    proxy()->drawPrimitive(PE_FrameLineEdit, option, painter, widget);
                }
            }

            if (option->subControls & SC_SpinBoxUp && option->subControls & SC_SpinBoxDown) {
                drawArrow(Up, painter, subControlRect(control, option, SC_SpinBoxUp, widget).translated(0, dp(2)), option->palette.color(QPalette::Text));
                drawArrow(Down, painter, subControlRect(control, option, SC_SpinBoxDown, widget).translated(0, -dp(2)), option->palette.color(QPalette::Text));
            }
            return;
        }
        break;
    case CC_ScrollBar:
        if (const QStyleOptionSlider *scrollBar = qstyleoption_cast<const QStyleOptionSlider*>(option)) {
            painter->save();

            QRect scrollBarSubLine = proxy()->subControlRect(control, scrollBar, SC_ScrollBarSubLine, widget);
            QRect scrollBarAddLine = proxy()->subControlRect(control, scrollBar, SC_ScrollBarAddLine, widget);
            QRect scrollBarSlider = proxy()->subControlRect(control, scrollBar, SC_ScrollBarSlider, widget);
            QRect scrollBarGroove = proxy()->subControlRect(control, scrollBar, SC_ScrollBarGroove, widget);

            if (scrollBar->subControls & SC_ScrollBarGroove)
                painter->fillRect(scrollBarGroove, scrollBar->palette.button());

            if (scrollBar->subControls & SC_ScrollBarSlider)
                painter->fillRect(scrollBarSlider, scrollBar->palette.midlight());

            painter->setPen(scrollBar->palette.color(QPalette::Button));

            if (scrollBar->subControls & SC_ScrollBarAddLine) {
                painter->fillRect(scrollBarAddLine, scrollBar->palette.midlight());
                if (scrollBar->orientation == Qt::Vertical) {
                    painter->drawLine(scrollBarAddLine.topLeft(), scrollBarAddLine.topRight());
                    drawArrow(Down, painter, scrollBarAddLine, scrollBar->palette.color(QPalette::Text));
                } else {
                    painter->drawLine(scrollBarAddLine.topLeft(), scrollBarAddLine.bottomLeft());
                    drawArrow(Right, painter, scrollBarAddLine, scrollBar->palette.color(QPalette::Text));
                }
            }

            if (scrollBar->subControls & SC_ScrollBarSubLine) {
                painter->fillRect(scrollBarSubLine, scrollBar->palette.midlight());
                if (scrollBar->orientation == Qt::Vertical) {
                    painter->drawLine(scrollBarSubLine.bottomLeft(), scrollBarSubLine.bottomRight());
                    drawArrow(Up, painter, scrollBarSubLine, scrollBar->palette.color(QPalette::Text));
                } else {
                    painter->drawLine(scrollBarSubLine.topRight(), scrollBarSubLine.bottomRight());
                    drawArrow(Left, painter, scrollBarSubLine, scrollBar->palette.color(QPalette::Text));
                }
            }

            painter->restore();
            return;
        }
        break;
    default:
        break;
    }

    base_type::drawComplexControl(control, option, painter, widget);
}

void QnNxStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    switch (element) {
    case CE_ComboBoxLabel:
        if (const QStyleOptionComboBox *comboBox = qstyleoption_cast<const QStyleOptionComboBox*>(option)) {
            QStyleOptionComboBox opt = *comboBox;
            opt.rect.setLeft(opt.rect.left() + dp(6));
            base_type::drawControl(element, &opt, painter, widget);
            return;
        }
        break;
    case CE_TabBarTabShape:
        if (const QStyleOptionTab *tab = qstyleoption_cast<const QStyleOptionTab*>(option)) {
            if (tab->state & QStyle::State_Selected) {
                QRect rect = tab->rect;
                rect.setTop(rect.bottom() - dp(2));
                rect.setBottom(rect.bottom() - dp(1));
                painter->fillRect(rect, tab->palette.highlight());
            }
            return;
        }
        break;
    case CE_TabBarTabLabel:
        if (const QStyleOptionTab *tab = qstyleoption_cast<const QStyleOptionTab*>(option)) {
            QRect rect = tab->rect;
            drawItemText(painter, rect, Qt::AlignCenter | Qt::TextHideMnemonic,
                         tab->palette, tab->state & QStyle::State_Enabled, tab->text,
                         tab->state & QStyle::State_Selected ? QPalette::Highlight : QPalette::WindowText);
            return;
        }
    case CE_HeaderSection:
        if (const QStyleOptionHeader *header = qstyleoption_cast<const QStyleOptionHeader*>(option)) {
            painter->fillRect(header->rect, header->palette.window());

            if (header->orientation == Qt::Horizontal && !header->text.isEmpty()) {
                painter->save();
                painter->setPen(header->palette.color(QPalette::Midlight).darker(150));
                QRect rect = header->rect;
                painter->drawLine(rect.left(), rect.bottom(), rect.right() - dp(16), rect.bottom());
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
                painter->drawLine(menuItemHPadding, y, menuItem->rect.right() - menuItemHPadding, y);
                painter->restore();
                break;
            }

            painter->save();

            bool enabled = menuItem->state & State_Enabled;
            bool selected = enabled && (menuItem->state & State_Selected);

            if (selected) {
                painter->fillRect(menuItem->rect, menuItem->palette.highlight());
            }

            int xPos = menuItemHPadding + dp(16);
            int y = menuItem->rect.y();

            int textFlags = Qt::AlignVCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
            if (!styleHint(SH_UnderlineShortcut, menuItem, widget, nullptr))
                textFlags |= Qt::TextHideMnemonic;

            QRect textRect(xPos, y + menuItemVPadding, menuItem->rect.width() - xPos - menuItemHPadding, menuItem->rect.height() - 2 * menuItemVPadding);

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
                drawMenuCheckMark(painter, QRect(menuItemHPadding, menuItem->rect.y(), dp(16), menuItem->rect.height()),
                                  menuItem->palette.color(selected ? QPalette::HighlightedText : QPalette::WindowText));
            }

            if (menuItem->menuItemType == QStyleOptionMenuItem::SubMenu) {
                drawArrow(Right, painter,
                          QRect(menuItem->rect.right() - menuItemVPadding - arrowSize, menuItem->rect.top(), arrowSize, menuItem->rect.height()),
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
                rect.setSize(QSize(comboBox->rect.height(), comboBox->rect.height()));
                rect.moveRight(comboBox->rect.right());
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
                    rect.setTop(labelRect.bottom() + dp(16));
                }
                break;
            }
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
    default:
        break;
    }

    return base_type::pixelMetric(metric, option, widget);
}

QSize QnNxStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size, const QWidget *widget) const {
    switch (type) {
    case CT_PushButton:
        return QSize(qMax(minimumButtonWidth, size.width() + 2 * pixelMetric(PM_ButtonMargin, option, widget)), qMax(size.height(), buttonHeight));
    case CT_LineEdit:
        return QSize(size.width(), qMax(size.height(), buttonHeight));
    case CT_ComboBox:
        return QSize(qMax(minimumButtonWidth, size.width() + 2 * pixelMetric(PM_ButtonMargin, option, widget)), qMax(size.height(), buttonHeight));
    case CT_TabBarTab:
        return QSize(size.width(), qMax(size.height(), (int)dp(40)));
    case CT_MenuItem:
        return QSize(size.width() + dp(24) + 2 * menuItemHPadding, size.height() + 2 * menuItemVPadding);
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

void QnNxStyle::polish(QWidget *widget) {
    base_type::polish(widget);

    if (qobject_cast<QAbstractButton*>(widget)) {
        QFont font = widget->font();
        font.setWeight(QFont::DemiBold);
        widget->setFont(font);
    }

    if (qobject_cast<QTabBar*>(widget)) {
        QFont font = widget->font();
        font.setPointSize(font.pointSize() - 1);
        widget->setFont(font);
    }

    if (qobject_cast<QMenu*>(widget)) {
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
