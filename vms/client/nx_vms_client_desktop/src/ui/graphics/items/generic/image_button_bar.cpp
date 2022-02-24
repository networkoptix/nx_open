// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "image_button_bar.h"

#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QAction>

#include <utils/math/math.h>
#include <utils/common/checked_cast.h>

#include <nx/utils/range_adapters.h>

#include "image_button_widget.h"

QnImageButtonBar::QnImageButtonBar(QGraphicsItem* parent, Qt::WindowFlags windowFlags, Qt::Orientation orientation):
    base_type(parent, windowFlags),
    m_visibleButtons(0),
    m_checkedButtons(0),
    m_enabledButtons(0),
    m_updating(false),
    m_submitting(false)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setAcceptedMouseButtons(Qt::NoButton);

    m_layout = new QGraphicsLinearLayout(orientation);
    m_layout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    m_layout->setSpacing(0.0);
    setLayout(m_layout);
}

QnImageButtonBar::~QnImageButtonBar()
{
}

void QnImageButtonBar::addButton(int mask, QnImageButtonWidget* button)
{
    if (!NX_ASSERT(button))
        return;

    button->setParentItem(this); //< We're expected to take ownership, even if the button wasn't actually added.
    button->setFocusPolicy(Qt::NoFocus); //< Button click should not take focus.

    if(!qIsPower2(mask))
        NX_ASSERT(false, "Given mask '%1' is not a power of 2.", mask);

    if(m_maskByButton.contains(button))
    {
        NX_ASSERT(false, "Given button is already added to this button bar.");
        return;
    }

    if(m_buttonByMask.contains(mask))
    {
        NX_ASSERT(false, "Given mask '%1' is already taken in this image button bar.", mask);
        return;
    }

    m_maskByButton[button] = mask;
    m_buttonByMask[mask] = button;

    connect(button, SIGNAL(visibleChanged()),   this, SLOT(at_button_visibleChanged()));
    connect(button, SIGNAL(toggled(bool)),      this, SLOT(at_button_toggled()));
    connect(button, SIGNAL(enabledChanged()),   this, SLOT(at_button_enabledChanged()));

    setButtonsVisible(mask, button->isVisible());
    setButtonsEnabled(mask, button->isEnabled());

    /* We should not emit signal while adding button as it may corrupt saved state. */
    setButtonsChecked(mask, button->isChecked(), true);

    submitButtonSize(button);
    submitVisibleButtons();
}

void QnImageButtonBar::removeButton(QnImageButtonWidget* button)
{
    if(!m_maskByButton.contains(button))
        return;

    if(button->parent() == this)
        button->setParent(nullptr); //< Release ownership.

    int mask = m_maskByButton.value(button);
    m_maskByButton.remove(button);
    m_buttonByMask.remove(mask);

    disconnect(button, nullptr, this, nullptr);

    submitVisibleButtons();
}

QnImageButtonWidget* QnImageButtonBar::button(int mask) const
{
    return m_buttonByMask.value(mask);
}

int QnImageButtonBar::visibleButtons() const
{
    return m_visibleButtons;
}

void QnImageButtonBar::setVisibleButtons(int visibleButtons)
{
    if(m_visibleButtons == visibleButtons)
        return;

    m_visibleButtons = visibleButtons;
    submitVisibleButtons();
    emit visibleButtonsChanged();
}

void QnImageButtonBar::setButtonsVisible(int mask, bool visible)
{
    setVisibleButtons(visible ? (m_visibleButtons | mask) : (m_visibleButtons & ~mask));
}

int QnImageButtonBar::checkedButtons() const
{
    return m_checkedButtons;
}

void QnImageButtonBar::setCheckedButtons(int checkedButtons, bool silent)
{
    if (m_checkedButtons == checkedButtons)
        return;

    for (auto pos = m_buttonByMask.begin(); pos != m_buttonByMask.end(); pos++)
    {
        if (!pos.value()->isCheckable())
            checkedButtons &= ~pos.key();
    }

    if (m_checkedButtons == checkedButtons)
        return;

    int changedButtons = m_checkedButtons ^ checkedButtons;
    m_checkedButtons = checkedButtons;

    // TODO: #sivanov We have a problem here.
    // if checked state changes during submit, we won't catch it.
    submitCheckedButtons(changedButtons);

    if (!silent)
        emit checkedButtonsChanged();
}

void QnImageButtonBar::setButtonsChecked(int mask, bool checked, bool silent)
{
    setCheckedButtons(checked ? (m_checkedButtons | mask) : (m_checkedButtons & ~mask), silent);
}

int QnImageButtonBar::enabledButtons() const
{
    return m_enabledButtons;
}

void QnImageButtonBar::setEnabledButtons(int enabledButtons)
{
    if (m_enabledButtons == enabledButtons)
        return;

    int changedButtons = m_enabledButtons ^ enabledButtons;
    m_enabledButtons = enabledButtons;

    submitEnabledButtons(changedButtons);

    emit enabledButtonsChanged();
}

void QnImageButtonBar::setButtonsEnabled(int mask, bool enabled)
{
    setEnabledButtons(enabled ? (m_enabledButtons | mask) : (m_enabledButtons & ~mask));
}

const QSizeF& QnImageButtonBar::uniformButtonSize() const
{
    return m_uniformButtonSize;
}

void QnImageButtonBar::setUniformButtonSize(const QSizeF& uniformButtonSize)
{
    if (m_uniformButtonSize == uniformButtonSize)
        return;

    m_uniformButtonSize = uniformButtonSize;

    foreach (QnImageButtonWidget* button, m_buttonByMask)
        submitButtonSize(button);
}

int QnImageButtonBar::unusedMask() const
{
    int usedMask = 0;
    foreach (int mask, m_maskByButton)
        usedMask |= mask;

    int mask = 1;
    for (int i = 0; i < 31; i++)
    {
        if (!(usedMask & mask))
            return mask;
        mask <<= 1;
    }

    return 0;
}

qreal QnImageButtonBar::spacing() const
{
    return m_layout->spacing();
}

void QnImageButtonBar::setSpacing(qreal value)
{
    m_layout->setSpacing(value);
}

void QnImageButtonBar::submitVisibleButtons()
{
    if (m_updating)
        return;

    QScopedValueRollback<bool> guard(m_submitting, true);

    for (auto button: m_buttonByMask)
    {
        m_layout->removeItem(button);
        if (auto action = button->defaultAction())
            action->setVisible(false);
        else
            button->setVisible(false);
    }

    for (auto button: m_buttonByMask)
    {
        const int mask = m_maskByButton.value(button);
        if ((mask & m_visibleButtons) == 0)
            continue;

        m_layout->insertItem(0, button);
        if (auto action = button->defaultAction())
            action->setVisible(true);
        else
            button->setVisible(true);
    }
}

void QnImageButtonBar::submitCheckedButtons(int mask)
{
    if (m_updating)
        return;

    QScopedValueRollback<bool> guard(m_submitting, true);

    for (auto [buttonMask, button]: nx::utils::constKeyValueRange(m_buttonByMask))
    {
        if ((buttonMask & mask) == 0)
            continue;

        const bool checked = (buttonMask & m_checkedButtons) != 0;
        if (auto action = button->defaultAction(); action && action->isCheckable())
            action->setChecked(checked);
        else
            button->setChecked(checked);
    }
}

void QnImageButtonBar::submitEnabledButtons(int mask)
{
    if (m_updating)
        return;

    QScopedValueRollback<bool> guard(m_submitting, true);

    for (auto [buttonMask, button]: nx::utils::constKeyValueRange(m_buttonByMask))
    {
        if ((buttonMask & mask) == 0)
            continue;

        const bool enabled = (buttonMask & m_enabledButtons) != 0;
        if (auto action = button->defaultAction())
            action->setEnabled(enabled);
        else
            button->setEnabled(enabled);
    }
}

void QnImageButtonBar::submitButtonSize(QnImageButtonWidget* button)
{
    if (m_uniformButtonSize.isEmpty())
        return;

    button->setFixedSize(m_uniformButtonSize);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnImageButtonBar::at_button_visibleChanged()
{
    if(m_submitting)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);
    auto button = checked_cast<QnImageButtonWidget*>(sender());
    setButtonsVisible(m_maskByButton.value(button), button->isVisible());
}

void QnImageButtonBar::at_button_toggled()
{
    if(m_submitting)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);
    auto button = checked_cast<QnImageButtonWidget*>(sender());
    setButtonsChecked(m_maskByButton.value(button), button->isChecked());
}

void QnImageButtonBar::at_button_enabledChanged()
{
    if(m_submitting)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);
    auto button = checked_cast<QnImageButtonWidget*>(sender());
    setButtonsEnabled(m_maskByButton.value(button), button->isEnabled());
}
