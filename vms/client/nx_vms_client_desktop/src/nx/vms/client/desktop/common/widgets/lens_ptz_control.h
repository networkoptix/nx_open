#pragma once

#include <QtWidgets/QWidget>
#include <QtCore/QTimer>
#include "ui/style/generic_palette.h"
#include <array>

namespace nx::vms::client::desktop {

/**
 * Magic circle to control PTZ+rotation.
 * It consists of central joystick for pan/tilt control, and circle slider to control rotation.
 * TODO: Fix logic for arrow buttons.
 */
class LensPtzControl: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    LensPtzControl(QWidget* parent);
    virtual ~LensPtzControl() override;

    virtual QSize sizeHint() const override;

    // Set if rotation control is enabled.
    void setRotationEnabled(bool value);
    bool rotationEnabled() const;

    // Set if pan/tilt control is enabled.
    void setPanTiltEnabled(bool value);
    bool panTiltEnabled() const;

    struct Value
    {
        float rotation = 0;
        float horizontal = 0;
        float vertical = 0;
    };

    Value value() const;
    void setValue(const Value& val);

    // Handlers for additional rotation buttons.
    // Note: we should track on/off state for both cw/ccw buttons.
    // It solves problems when we can press both buttons simultaneously, i.e
    // in case of multitouch or when this buttons are binded to the keyboard.
    void onRotationButtonCounterClockWise(bool pressed);
    void onRotationButtonClockWise(bool pressed);

signals:
    void valueChanged(const Value& value);

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;

protected:
    // Works like button, but without soul of a Qt widget.
    struct Button
    {
        QRect rect;
        bool isHovered = false;
        bool isClicked = false;

        // Changes 'clicked' state. Returns true if state has been changed.
        bool setClicked(bool value);
        // Changes 'hovered' state. Returns true if state has been changed.
        bool setHover(bool value);
        bool picks(const QPointF& point) const;

        QPixmap normal;
        QPixmap hover;
    };

    enum ButtonType
    {
        ButtonLeft,
        ButtonRight,
        ButtonUp,
        ButtonDown,
        ButtonMax
    };

    std::array<Button, ButtonMax> m_buttons;

    // Joystick handler.
    // Another soulless widget.
    struct Handler
    {
        QPointF position;
        bool picked = false;
        bool hover = false;
        // Radius of the handler.
        float radius;
        // Max distance the handler can be dragged from the center.
        float maxDistance = -1;
        // Min distance the handler can be dragged from the center.
        float minDistance = -1;

        bool dragTo(const QPointF& point);
        bool picks(const QPointF& point) const;

        bool setHover(bool value);
    };

    void updateState();

    void onButtonClicked(ButtonType button, bool state);

    void drawRotationCircle(QPainter* painter, const QRectF& rect) const;
    void drawRotationValue(QPainter* painter, const QRectF& rect, float rotation) const;
    void drawHandler(QPainter* painter, const Handler& handler) const;
    void drawButton(QPainter* painter, const Button& button) const;

    // Converts screen cordinates to local coordinates, relative to control's center.
    QPointF screenToLocal(const QPointF& pos) const;

    enum State
    {
        StateInitial,
        StateHandlePtz,
        StateHandleRotation,
        StateHoldButton,
    };

    State m_state = StateInitial;
    // Radius for the control.
    // It is updated every time widget was resized.
    float m_radius;

    // Current state of control.
    Value m_current;
    // Combined value of internal state and button state.
    // We send this value out, in value() and valueChanged(...).
    Value m_output;

    // Additional change from buttons,
    Value m_buttonState;
    // Increment for horizontal or vertical axis.
    // It is applied when you press arrow buttons on the widget.
    float m_increment = 0.1f;

    Handler m_ptzHandler;
    Handler m_rotationHandler;

    bool m_rotationEnabled = true;
    bool m_rotationIsAbsolute = false;
    bool m_rotationAutoReturn = true;
    bool m_panTiltEnabled = true;

    QnGenericPalette m_palette;

    bool m_needRedraw = false;
    QTimer* m_timer = nullptr;
};

} // namespace nx::vms::client::desktop
