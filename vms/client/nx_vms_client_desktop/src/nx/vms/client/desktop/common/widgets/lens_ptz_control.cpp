#include <QtCore/QEvent>
#include <QtGui/QPainter>
#include <QtGui/QVector2D>
#include <QtWidgets/QStylePainter>
#include <QtWidgets/QStyleOption>
#include <QtGui/QMouseEvent>
#include <QtCore/QtMath>

#include "lens_ptz_control.h"

#include <ui/style/nx_style.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>
#include "nx/utils/literal.h"
#include <ui/style/skin.h>

namespace {
    // Main width for all lines.
    const int kLineWidth = 2;
    const int kLineBorderWidth = 1;
    const int kHandlerDiameter = 16;
    const int kDotWidth = kLineWidth * 2;
    const int kButtonSize = 20;
    // Offset from center of the circle to center of directional button.
    const int kButtonOffset = 10 + kButtonSize;

    // Return time in milliseconds. This time is needed to
    // auto-return the handler to its initial position.
    const int kReturnTime = 500;

    const qreal kSensitivity = 0.02;
    // Minimal size for joystick circle.
    const int kMinCircleSize = 176;

    // Time period to read updates for ptzr control.
    const int kUpdatePeriod = 33;

    // Increments to be applied when we press appropriate button.
    const float kPanTiltIncrement = 1.0;
    const float kRotationIncrement = 90;

    const QString kNormalIcons[] =
    {
        lit("text_buttons/arrow_left.png"),
        lit("text_buttons/arrow_right.png"),
        lit("text_buttons/arrow_up.png"),
        lit("text_buttons/arrow_down.png"),
    };

    const QString kHoveredIcons[] =
    {
        lit("text_buttons/arrow_left_hovered.png"),
        lit("text_buttons/arrow_right_hovered.png"),
        lit("text_buttons/arrow_up_hovered.png"),
        lit("text_buttons/arrow_down_hovered.png"),
    };

    inline qreal length(const QPointF& point)
    {
        return qSqrt(QPointF::dotProduct(point, point));
    }
} // namespace

namespace nx::vms::client::desktop {

LensPtzControl::LensPtzControl(QWidget* parent):
    QWidget(parent)
{
    auto nxStyle = QnNxStyle::instance();
    NX_ASSERT(nxStyle);

    if (nxStyle)
        m_palette = nxStyle->genericPalette();

    for (size_t i = 0; i < m_buttons.size(); ++i)
    {
        m_buttons[i].normal = qnSkin->pixmap(kNormalIcons[i], true);
        m_buttons[i].hover = qnSkin->pixmap(kHoveredIcons[i], true);
    }

    m_rotationHandler.radius = kHandlerDiameter / 2;
    m_rotationHandler.position.setY(kHandlerDiameter / 2);
    m_ptzHandler.radius = kHandlerDiameter / 2;
    // We should set this limits or strange things will happen.
    m_ptzHandler.maxDistance = 1.0;
    m_rotationHandler.maxDistance = 1.0;
    m_rotationHandler.minDistance = 1.0;

    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setMinimumSize(QSize(kMinCircleSize, kMinCircleSize));

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &LensPtzControl::updateState);
    m_timer->start(kUpdatePeriod);
}

LensPtzControl::~LensPtzControl()
{
}

template<class T> T clamp(T value, T limit)
{
    if (value > limit)
        return limit;
    else if (value < -limit)
        return -limit;
    return value;
}

bool changedEnough(qreal oldValue, qreal newValue, qreal threshold)
{
    // Check if delta is large enough, or we crossed zero
    return qAbs(newValue - oldValue) >= threshold || ((oldValue != 0) != (newValue != 0));
}

void LensPtzControl::updateState()
{
    // Auto return central handler if it is released.
    if (m_state != StateHandlePtz)
    {
        auto distance = length(m_ptzHandler.position);
        qreal returnSpeed = m_ptzHandler.maxDistance * kUpdatePeriod / kReturnTime;

        if (distance > 0)
        {
            if (distance > returnSpeed)
            {
                float newDistance = distance - returnSpeed;
                m_ptzHandler.position *= newDistance / distance;
            }
            else
            {
                m_ptzHandler.position = QPointF(0, 0);
            }
            m_needRedraw = true;
        }
    }

    Value newValue; //< value from handlers

    newValue.rotation = m_current.rotation;
    if (int len2 = QPointF::dotProduct(m_rotationHandler.position, m_rotationHandler.position))
    {
        newValue.rotation = qAtan2(m_rotationHandler.position.x(), m_rotationHandler.position.y());
        newValue.rotation = qRadiansToDegrees(newValue.rotation);
    }

    // Auto-return for rotation.
    if (m_state != StateHandleRotation && m_rotationAutoReturn)
    {
        qreal returnSpeed = 180.0 * kUpdatePeriod / kReturnTime;

        if (qAbs(newValue.rotation) > 0)
        {
            if (qAbs(newValue.rotation) > returnSpeed)
                newValue.rotation -= (newValue.rotation > 0 ? returnSpeed : -returnSpeed);
            else
                newValue.rotation = 0;
            m_needRedraw = true;
        }

        // Adjusting screen position of rotational handler to a new angle.
        QPointF newPosition(qSin(qDegreesToRadians(newValue.rotation)),
            qCos(qDegreesToRadians(newValue.rotation)));
        m_rotationHandler.dragTo(newPosition * m_rotationHandler.maxDistance);
    }

    // We use 2d vector to ease calculations.
    if (m_state != StateInitial)
    {
        float limit = m_ptzHandler.maxDistance > 0 ? m_ptzHandler.maxDistance : 1.0;
        newValue.horizontal = -m_ptzHandler.position.x() / limit;
        newValue.vertical = m_ptzHandler.position.y() / limit;
    }

    Value newOutput = newValue; //< value from handlers and external buttons.

    // Adding values from pressed buttons.
    newOutput.horizontal += m_buttonState.horizontal;
    newOutput.vertical += m_buttonState.vertical;
    newOutput.rotation += m_buttonState.rotation;

    newOutput.horizontal = clamp(newOutput.horizontal, 1.0f);
    newOutput.vertical = clamp(newOutput.vertical, 1.0f);
    newOutput.rotation = clamp(newOutput.rotation, 180.0f);

    // Should send signal if we:
    //  - pan/tilt has changed over the threshold.
    //  - pan/tilt became zero, and was not zero before.
    //  - pan/tilt became non-zero and was zero before.
    //  - all the same for rotation.
    bool changed = changedEnough(m_output.horizontal, newOutput.horizontal, kSensitivity)
        || changedEnough(m_output.vertical, newOutput.vertical, kSensitivity)
        || changedEnough(m_output.rotation, newOutput.rotation, kSensitivity*180.0);

    if (changed)
    {
        m_current = newValue;
        m_output = newOutput;
        emit valueChanged(m_output);
    }

    if (m_needRedraw)
        update();
}

void LensPtzControl::onButtonClicked(ButtonType button, bool state)
{
    if (m_state != StateInitial)
        return;

    const auto delta = state ? kPanTiltIncrement : -kPanTiltIncrement;

    switch (button)
    {
        case ButtonLeft:
            m_buttonState.horizontal += delta;
            break;
        case ButtonRight:
            m_buttonState.horizontal -= delta;
            break;
        case ButtonUp:
            m_buttonState.vertical += delta;
            break;
        case ButtonDown:
            m_buttonState.vertical -= delta;
            break;
        default:
            break;
    }
}

void LensPtzControl::onRotationButtonCounterClockWise(bool pressed)
{
    m_buttonState.rotation += pressed ? kRotationIncrement : -kRotationIncrement;
}

void LensPtzControl::onRotationButtonClockWise(bool pressed)
{
    m_buttonState.rotation -= pressed ? kRotationIncrement : -kRotationIncrement;
}

QSize LensPtzControl::sizeHint() const
{
    QSize result = base_type::minimumSizeHint();
    if (result.isEmpty())
        result = QSize(kMinCircleSize, kMinCircleSize); /* So that we don't end up with an invalid size on the next step. */

    return result;
}

void LensPtzControl::resizeEvent(QResizeEvent* event)
{
    base_type::resizeEvent(event);

    QPoint center(width() / 2, height() / 2);
    m_radius = std::min(center.x(), center.y()) - kLineWidth - kLineBorderWidth - kHandlerDiameter;
    m_ptzHandler.maxDistance = m_radius;
    m_rotationHandler.maxDistance = m_radius;
    m_rotationHandler.minDistance = m_radius;

    if (m_state != StateHandleRotation)
    {
        // We should preserve current rotation using such transform
        m_rotationHandler.dragTo(m_rotationHandler.position);
    }

    // Adjusting internal buttons
    QRect centerRect(0, 0, kButtonSize, kButtonSize);
    centerRect.moveCenter(center);
    m_buttons[ButtonLeft].rect = centerRect.translated(-kButtonOffset, 0);
    m_buttons[ButtonRight].rect = centerRect.translated(kButtonOffset, 0);
    m_buttons[ButtonUp].rect = centerRect.translated(0, -kButtonOffset);
    m_buttons[ButtonDown].rect = centerRect.translated(0, kButtonOffset);
}

void LensPtzControl::mousePressEvent(QMouseEvent* event)
{
    if (m_state != StateInitial)
        return;

    if (!(event->buttons().testFlag(Qt::LeftButton)))
        return;

    QPointF localPos = screenToLocal(event->pos());

    if (m_panTiltEnabled && m_ptzHandler.picks(localPos))
    {
        m_state = StateHandlePtz;
        m_ptzHandler.picked = true;
        m_needRedraw = true;
    }
    else if (m_rotationEnabled && m_rotationHandler.picks(localPos))
    {
        m_state = StateHandleRotation;
        m_rotationHandler.picked = true;
        m_needRedraw = true;
    }

    if (m_panTiltEnabled)
    {
        // Buttons use widget coordinates
        auto pos = event->pos();
        for (int i = 0; i < ButtonMax; ++i)
        {
            if (m_buttons[i].picks(pos))
            {
                m_buttons[i].isClicked = true;
                onButtonClicked((ButtonType)i, true);
            }
        }
    }

    if (m_needRedraw)
        update();
}

void LensPtzControl::mouseReleaseEvent(QMouseEvent* /*event*/)
{
    for (int i = 0; i < ButtonMax; ++i)
    {
        if (m_buttons[i].isClicked)
        {
            m_buttons[i].isClicked = false;
            onButtonClicked((ButtonType)i, false);
        }
    }

    if (m_state == StateInitial)
    {
        return;
    }
    else if (m_state == StateHandlePtz)
    {
        m_ptzHandler.picked = false;
        m_needRedraw = true;
    }
    else if (m_state == StateHandleRotation)
    {
        m_rotationHandler.picked = false;
        m_needRedraw = true;
    }
    m_state = StateInitial;
}

void LensPtzControl::mouseMoveEvent(QMouseEvent* event)
{
    QPointF localPos = screenToLocal(event->pos());

    if (m_state == StateHandlePtz)
    {
        m_needRedraw |= m_ptzHandler.dragTo(localPos);
    }
    else if (m_state == StateHandleRotation)
    {
        // Rotation handler is limited to its circle.
        if (!m_rotationHandler.dragTo(localPos))
        {
            // TODO: Reset to initial state
        }

        m_needRedraw = true;
    }

    if (m_panTiltEnabled)
    {
        auto pos = event->pos();
        for (auto& button : m_buttons)
            m_needRedraw |= button.setHover(button.picks(pos));

        m_needRedraw |= m_ptzHandler.setHover(m_ptzHandler.picks(pos));
    }

    if (m_rotationEnabled)
        m_needRedraw |= m_rotationHandler.setHover(m_rotationHandler.picks(localPos));

    // Need to redraw handlers when we move them
    if (m_state != StateInitial)
        m_needRedraw = true;
    if (m_needRedraw)
        update();
}

LensPtzControl::Value LensPtzControl::value() const
{
    return m_output;
}

void LensPtzControl::setValue(const Value& value)
{
    m_current = value;
}

void LensPtzControl::setRotationEnabled(bool value)
{
    if (m_rotationEnabled == value)
        return;
    m_rotationEnabled = value;
    update();
}

bool LensPtzControl::rotationEnabled() const
{
    return m_rotationEnabled;
}

void LensPtzControl::setPanTiltEnabled(bool value)
{
    if (m_panTiltEnabled == value)
        return;
    m_panTiltEnabled = value;
    update();
}

bool LensPtzControl::panTiltEnabled() const
{
    return m_panTiltEnabled;
}

void LensPtzControl::paintEvent(QPaintEvent* event)
{
    base_type::paintEvent(event);

    QRectF rect(QPointF(0, 0), size());

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    drawRotationCircle(&painter, rect);

    if (m_rotationEnabled)
    {
        drawRotationValue(&painter, rect, m_current.rotation);
        drawHandler(&painter, m_rotationHandler);
    }

    if (m_panTiltEnabled)
    {
        for (const auto& button: m_buttons)
            drawButton(&painter, button);
        drawHandler(&painter, m_ptzHandler);
    }

    m_needRedraw = false;
}

void LensPtzControl::drawRotationCircle(QPainter* painter, const QRectF& rect) const
{
    QPointF center = rect.center();
    QRectF centered(QPointF(0, 0), QSizeF(m_radius, m_radius) * 2);
    centered.moveCenter(center);

    // Drawing circle with border color.
    QnPaletteColor borderColor = m_palette.color(lit("dark"), 5);
    QPen reallyWide(QBrush(borderColor), kLineWidth + 2 * kLineBorderWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(reallyWide);
    painter->drawArc(centered, 0, 16 * 360);

    // Drawing full circle with main color.
    QnPaletteColor mainColor = m_palette.color(lit("dark"), 11);
    QPen thinPen(QBrush(mainColor), kLineWidth);
    painter->setPen(thinPen);
    painter->drawArc(centered, 0, 16 * 360);
}

void LensPtzControl::drawRotationValue(QPainter* painter, const QRectF& rect, float rotation) const
{
    QPointF center = rect.center();
    QRectF centered(QPointF(0, 0), QSizeF(m_radius, m_radius) * 2);
    centered.moveCenter(center);
    // Drawing current 'value'.
    QColor fillColor = palette().color(QPalette::WindowText);
    QPen filledPen(QBrush(fillColor), kLineWidth);
    painter->setPen(filledPen);
    int startAngle = 270 * 16;
    painter->drawArc(centered, startAngle, 16 * rotation);

    QRectF dotRect(0, 0, kDotWidth, kDotWidth);
    painter->setBrush(QBrush(fillColor));
    dotRect.moveCenter(center);
    // Draw dot at the center.
    painter->drawEllipse(dotRect);
    // Draw lower dot.
    dotRect.moveCenter(center + QPointF(0, m_radius));
    painter->drawEllipse(dotRect);
}

void LensPtzControl::drawHandler(QPainter* painter, const Handler& handler) const
{
    QStyleOptionSlider sliderOpts;
    sliderOpts.initFrom(this);

    sliderOpts.state = QStyle::State_None;
    if (isEnabled())
        sliderOpts.state |= QStyle::State_Enabled;
    if (hasFocus())
        sliderOpts.state |= QStyle::State_HasFocus;
    if (isActiveWindow())
        sliderOpts.state |= QStyle::State_Active;

    sliderOpts.subControls = QStyle::SC_None;
    sliderOpts.activeSubControls = QStyle::SC_None;
    sliderOpts.maximum = sliderOpts.minimum = 0;
    sliderOpts.tickPosition = QSlider::NoTicks;
    sliderOpts.sliderPosition = 0;
    sliderOpts.sliderValue = 0;
    sliderOpts.singleStep = 0;
    sliderOpts.pageStep = 0;

    QPointF center(width() / 2, height() / 2);
    QPointF handlerScreenPos = center + handler.position;
    QRectF rect(0, 0, kHandlerDiameter, kHandlerDiameter);
    rect.moveCenter(handlerScreenPos);
    sliderOpts.rect = rect.toRect();
    sliderOpts.subControls = QStyle::SC_SliderHandle;

    if (handler.hover)
        sliderOpts.state |= QStyle::State_MouseOver;

    if (handler.picked)
    {
        sliderOpts.state |= QStyle::State_Sunken;
        sliderOpts.activeSubControls = QStyle::SC_SliderHandle;
    }

    style()->drawComplexControl(QStyle::CC_Slider, &sliderOpts, painter, this);
}

void LensPtzControl::drawButton(QPainter* painter, const Button& button) const
{
    const QPixmap& pixmap = (button.isClicked != button.isHovered) ? button.hover : button.normal;

    if (!pixmap.isNull())
        painter->drawPixmap(button.rect, pixmap);
}

QPointF LensPtzControl::screenToLocal(const QPointF& pos) const
{
    QPointF center(width() / 2, height() / 2);
    return pos - center;
}

bool LensPtzControl::Button::setClicked(bool value)
{
    if (value == isClicked)
        return false;
    isClicked = value;
    return true;
}

bool LensPtzControl::Button::setHover(bool value)
{
    if (value == isHovered)
        return false;
    isHovered = value;
    return true;
}

bool LensPtzControl::Button::picks(const QPointF& point) const
{
    return rect.contains(point.toPoint());
}

bool LensPtzControl::Handler::dragTo(const QPointF& point)
{
    const auto distance = length(point);

    position = point;
    if (distance > 0)
    {
        if (maxDistance > 0.f && distance > maxDistance)
        {
            position = position * maxDistance / distance;
        }
        // Distance could be zero but min distance could be larger.
        // Should reset to default position.
        if (minDistance > 0.f && distance < minDistance)
        {
            position = position * minDistance / distance;
        }
    }
    else
    {
        position = QPoint(0, maxDistance);
    }
    // if (minDistance > 0)

    return true;
}

bool LensPtzControl::Handler::setHover(bool value)
{
    if (hover == value)
        return false;

    hover = value;
    return true;
}

bool LensPtzControl::Handler::picks(const QPointF& mouse) const
{
    QPointF delta = mouse - position;

    qreal len2 = QPointF::dotProduct(delta, delta);
    return len2 < radius * radius;
}

} // namespace nx::vms::client::desktop
