// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "password_preview_button.h"

#include <nx/vms/client/desktop/ini.h>

#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/widgets/line_edit_controls.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>

namespace nx::vms::client::desktop {

PasswordPreviewButton::PasswordPreviewButton(QWidget* parent):
    base_type(parent)
{
    setFlat(true);
    setIcon(qnSkin->icon("text_buttons/eye_closed_20.svg", "text_buttons/eye_open_20.svg"));
    setFixedSize(core::Skin::maximumSize(icon()));
    setCheckable(true);
    updateVisibility();

    connect(this, &QPushButton::pressed, this, &PasswordPreviewButton::updateEchoMode);
    connect(this, &QPushButton::released, this, &PasswordPreviewButton::updateEchoMode);
}

PasswordPreviewButton* PasswordPreviewButton::createInline(
    QLineEdit* parent,
    Validator visibilityCondition)
{
    auto button = new PasswordPreviewButton(parent);
    button->setLineEdit(parent);
    button->setCursor(Qt::ArrowCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setVisibilityCondition(visibilityCondition);

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

bool PasswordPreviewButton::isActivated() const
{
    return isChecked();
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
    setVisible(m_alwaysVisible
        || (m_lineEdit && !m_lineEdit->text().isEmpty()
            && (!m_visibilityCondition || m_visibilityCondition())));
}

void PasswordPreviewButton::setVisibilityCondition(Validator visibilityCondition)
{
    m_visibilityCondition = visibilityCondition;
}

void PasswordPreviewButton::paintEvent(QPaintEvent* /*event*/)
{
    const bool enabled = isEnabled();
    const bool activated = enabled && isActivated();
    const bool hovered = enabled && underMouse() && !isDown();

    const QIcon::State state = activated ? QIcon::On : QIcon::Off;
    const QIcon::Mode mode = hovered ? QIcon::Active : QIcon::Normal;

    const QSize size = core::Skin::maximumSize(icon());
    const QRect rect = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size, this->rect());

    QPainter painter(this);
    QnScopedPainterOpacityRollback opacityRollback(&painter,
        enabled ? 1.0 : style::Hints::kDisabledItemOpacity);

    icon().paint(&painter, rect, Qt::AlignCenter, mode, state);
}

} // namespace nx::vms::client::desktop
