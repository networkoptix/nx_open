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
    // Return speed of central handler. Pixels per timer's tick.
    const int kReturnSpeed = 20;

    const qreal kSensitivity = 0.02;
    // Minimal size for joystick circle.
    const int kMinCircleSize = 176;

    const int kUpdatePeriod = 33;

    const QString kNormalIcons[] =
    {
        lit("buttons/arrow_left.png"),
        lit("buttons/arrow_right.png"),
        lit("buttons/arrow_up.png"),
        lit("buttons/arrow_down.png"),
    };

    const QString kHoveredIcons[] =
    {
        lit("buttons/arrow_left_hovered.png"),
        lit("buttons/arrow_right_hovered.png"),
        lit("buttons/arrow_up_hovered.png"),
        lit("buttons/arrow_down_hovered.png"),
    };
} // namespace

namespace nx {
namespace client {
namespace desktop {

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
    m_ptzHandler.radius = kHandlerDiameter / 2;

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

void LensPtzControl::updateState()
{
    // Auto return central handler if it is released
    if (m_state != StateHandlePtz)
    {
        if (int len2 = QPointF::dotProduct(m_ptzHandler.position, m_ptzHandler.position))
        {
            float distance = sqrt(len2);
            if (distance > kReturnSpeed)
            {
                float newDistance = distance - kReturnSpeed;
                m_ptzHandler.position *= newDistance / distance;
            }
            else
            {
                m_ptzHandler.position = QPointF(0, 0);
            }
            m_needRedraw = true;
        }
    }

    float newRotation = m_current.rotation;
    if (int len2 = QPointF::dotProduct(m_rotationHandler.position, m_rotationHandler.position))
    {
        newRotation = qAtan2(m_rotationHandler.position.x(), m_rotationHandler.position.y());
        newRotation = qRadiansToDegrees(newRotation);
    }

    // We use 2d vector to ease calculations.
    QVector2D newValue;
    if (m_state != StateInitial)
    {
        float limit = m_ptzHandler.maxDistance > 0 ? m_ptzHandler.maxDistance : 1.0;
        newValue.setX(clamp(-m_ptzHandler.position.x() / limit, 1.0));
        newValue.setY(clamp(m_ptzHandler.position.y() / limit, 1.0));
    }

    QVector2D oldValue(m_current.horizontal, m_current.vertical);
    bool wasZero = oldValue.lengthSquared() == 0.f;
    bool becameZero = newValue.lengthSquared() == 0.f;
    QVector2D delta = newValue - oldValue;
    // Should send signal if we:
    //  - value has changed.
    //  - value became zero, and was not zero before.
    //  - value became non-zero and was zero before.
    //  - rotation has changed over the threshold
    if (delta.length() > kSensitivity || wasZero != becameZero || qFabs(newRotation - m_current.rotation) > kSensitivity*180.0)
    {
        m_current.horizontal = newValue.x();
        m_current.vertical = newValue.y();
        m_current.rotation = newRotation;
        emit valueChanged(m_current);
    }

    if (m_needRedraw)
        update();
}

void LensPtzControl::onButtonClicked(ButtonType button)
{
    if (m_state != StateInitial)
        return;
    // TODO: Implement
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

/*
void LensPtzControl::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::FontChange)
        updateGeometry();

    base_type::changeEvent(event);
}*/

void LensPtzControl::mousePressEvent(QMouseEvent* event)
{
    const int controlBtn = Qt::LeftButton;
    QPointF localPos = screenToLocal(event->pos());

    if (m_state != StateInitial)
        return;

    if (!(event->button() & controlBtn))
        return;

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
        for (int i = 0; i < ButtonMax; ++i)
            if (m_buttons[i].picks(localPos))
                onButtonClicked((ButtonType)i);
    }

    if (m_needRedraw)
        update();
}

void LensPtzControl::mouseReleaseEvent(QMouseEvent* event)
{
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
        for (auto& button : m_buttons)
            m_needRedraw |= button.setHover(button.picks(localPos));

        m_needRedraw |= m_ptzHandler.setHover(m_ptzHandler.picks(localPos));
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
    return m_current;
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
    QRectF centered(QPointF(0.f, 0.f), QSizeF(m_radius, m_radius) * 2);
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
    QRectF centered(QPointF(0.f, 0.f), QSizeF(m_radius, m_radius) * 2);
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
    {
        auto icon = pixmap.rect();
        QPointF centeredCorner = button.rect.center() - icon.center();
        painter->drawPixmap(centeredCorner, pixmap);
    }
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
    return rect.contains(point.x(), point.y());
}

bool LensPtzControl::Handler::dragTo(const QPointF& point)
{
    qreal distance = qSqrt(QPointF::dotProduct(point, point));

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

} // namespace desktop
} // namespace client
} // namespace nx
