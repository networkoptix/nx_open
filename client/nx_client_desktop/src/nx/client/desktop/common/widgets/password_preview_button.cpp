#include "password_preview_button.h"

#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>

#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>

#include <nx/client/desktop/common/widgets/line_edit_controls.h>
#include <nx/utils/log/assert.h>

namespace nx::client::desktop {

PasswordPreviewButton::PasswordPreviewButton(QWidget* parent):
    base_type(parent)
{
    setFlat(true);
    setIcon(qnSkin->icon(lit("text_buttons/eye.png")));
    setFixedSize(QnSkin::maximumSize(icon()));
    setCheckable(m_activationMode == ActivationMode::toggle);
    updateVisibility();

    connect(this, &QPushButton::pressed, this, &PasswordPreviewButton::updateEchoMode);
    connect(this, &QPushButton::released, this, &PasswordPreviewButton::updateEchoMode);
}

PasswordPreviewButton* PasswordPreviewButton::createInline(QLineEdit* parent)
{
    auto button = new PasswordPreviewButton(parent);
    button->setLineEdit(parent);
    button->setCursor(Qt::ArrowCursor);
    button->setFocusPolicy(Qt::NoFocus);

    LineEditControls::get(parent)->addControl(button);
    return button;
}

QLineEdit* PasswordPreviewButton::lineEdit() const
{
    return m_lineEdit;
}

void PasswordPreviewButton::setLineEdit(QLineEdit* value)
{
    if (m_lineEdit == value)
        return;

    if (m_lineEdit)
    {
        m_lineEdit->setEchoMode(QLineEdit::Password);
        m_lineEdit->disconnect(this);
    }

    m_lineEdit = value;

    updateEchoMode();
    updateVisibility();

    if (m_lineEdit)
    {
        connect(m_lineEdit, &QLineEdit::textChanged,
            this, &PasswordPreviewButton::updateVisibility);
    }
}

PasswordPreviewButton::ActivationMode PasswordPreviewButton::activationMode() const
{
    return m_activationMode;
}

void PasswordPreviewButton::setActivationMode(ActivationMode value)
{
    if (m_activationMode == value)
        return;

    m_activationMode = value;
    updateEchoMode();

    setChecked(false);
    setCheckable(m_activationMode == ActivationMode::toggle);
}

bool PasswordPreviewButton::isActivated() const
{
    switch (m_activationMode)
    {
        case ActivationMode::press:
            return isDown();

        case ActivationMode::hover:
            return underMouse();

        case ActivationMode::toggle:
            return isChecked();
    }

    NX_ASSERT(false, "Should never get here.");
    return false;
}

bool PasswordPreviewButton::alwaysVisible() const
{
    return m_alwaysVisible;
}

void PasswordPreviewButton::setAlwaysVisible(bool value)
{
    if (m_alwaysVisible == value)
        return;

    m_alwaysVisible = value;
    updateVisibility();
}

void PasswordPreviewButton::updateEchoMode()
{
    if (m_lineEdit)
        m_lineEdit->setEchoMode(isActivated() ? QLineEdit::Normal : QLineEdit::Password);
}

void PasswordPreviewButton::updateVisibility()
{
    setVisible(m_alwaysVisible || (m_lineEdit && !m_lineEdit->text().isEmpty()));
}

bool PasswordPreviewButton::event(QEvent* event)
{
    if (m_activationMode != ActivationMode::hover)
        return base_type::event(event);

    switch (event->type())
    {
        case QEvent::Enter:
        case QEvent::Leave:
        case QEvent::HoverEnter:
        case QEvent::HoverLeave:
            updateEchoMode();
            break;

        default:
            break;
    }

    return base_type::event(event);
}

void PasswordPreviewButton::paintEvent(QPaintEvent* /*event*/)
{
    const bool enabled = isEnabled();
    const bool activated = enabled && isActivated();
    const bool highlighted = enabled && underMouse() && !isDown()
        && (!activated || m_activationMode == ActivationMode::toggle);

    const QIcon::State state = activated ? QIcon::On : QIcon::Off;
    const QIcon::Mode mode = highlighted ? QIcon::Active : QIcon::Normal;

    const QSize size = QnSkin::maximumSize(icon());
    const QRect rect = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size, this->rect());

    QPainter painter(this);
    QnScopedPainterOpacityRollback opacityRollback(&painter,
        enabled ? 1.0 : style::Hints::kDisabledItemOpacity);

    icon().paint(&painter, rect, Qt::AlignCenter, mode, state);
}

} // namespace nx::client::desktop
