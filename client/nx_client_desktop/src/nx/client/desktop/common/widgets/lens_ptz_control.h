#pragma once

#include <QtWidgets/QWidget>
#include <QTimer>
#include "ui/style/generic_palette.h"
#include <array>

namespace nx {
namespace client {
namespace desktop {

/**
 * Magic circle to control PTZ+rotation.
 * It consists of central joystick for pan/tilt control, and circle slider to control rotation.
 * TODO: Make proper events for changed pan/tilt and rotation
 * TODO: Fix handler for rotation. Now it starts at the center
 * TODO: Fix logic for arrow buttons
 */
class LensPtzControl: public QWidget
{
    Q_OBJECT;
    using base_type = QWidget;

public:
    LensPtzControl(QWidget* parent);
    virtual ~LensPtzControl();

    struct Value
    {
        float rotation;
        float horisontal;
        float vertical;
    };

signals:
    void onUpdateValue(const Value& value);

protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;
    QSize minimumSizeHint() const override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

protected:

    // Works like button, but without soul of Qt widget
    struct Button
    {
        QRect rect;
        bool isHovered = false;
        bool isClicked = false;

        bool setClicked(bool value);
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

    // Joystick handler
    struct Handler
    {
        QPointF position;
        bool picked = false;
        bool hover = false;
        // Radius of the handler
        float radius;
        // Max distance the handler can be dragged from the center
        float maxDistance = -1;
        // Min distance the handler can be dragged from the center
        float minDistance = -1;

        bool dragTo(const QPointF& point);
        bool picks(const QPointF& point) const;

        bool setHover(bool value);
    };

    void updateState();

    void drawRotationCircle(QPainter& painter, const QRectF& rect, float rotation) const;
    void drawHandler(QPainter& painter, const Handler& handler) const;
    void drawButton(QPainter& painter, const Button& button) const;

    // Converts screen cordinates to local coordinates, relative to control's center
    QPointF screenToLocal(const QPointF& pos) const;

    enum State
    {
        StateInitial,
        StateHandlePtz,
        StateHandleRotation,
    };

    State m_state = StateInitial;
    // Radius for the control
    float m_radius;
    // Current state for rotation
    float m_rotation = 0.0;

    Handler m_ptzHandler;
    Handler m_rotationHandler;

    QnGenericPalette m_palette;

    bool m_needRedraw = false;
    QTimer* m_timer = nullptr;
};

} // namespace desktop
} // namespace client
} // namespace nx
