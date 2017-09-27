#include "selectable_text_button_group.h"

#include <nx/utils/raii_guard.h>
#include <nx/client/desktop/ui/common/selectable_text_button.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

SelectableTextButtonGroup::SelectableTextButtonGroup(QObject* parent):
    base_type(parent)
{
}

SelectableTextButtonGroup::~SelectableTextButtonGroup()
{
}

bool SelectableTextButtonGroup::add(SelectableTextButton* button)
{
    if (!button)
        return false;

    if (m_buttons.contains(button))
        return false;

    m_buttons.insert(button);

    connect(button, &SelectableTextButton::destroyed, this,
        [this]() { remove(static_cast<SelectableTextButton*>(sender())); });

    connect(button, &SelectableTextButton::stateChanged, this,
        [this](SelectableTextButton::State state)
        {
            const auto button = static_cast<SelectableTextButton*>(sender());
            emit buttonStateChanged(button);

            if (m_selectionChanging)
                return;

            if (state == SelectableTextButton::State::selected)
                setSelected(button);
            else if (button == m_selected)
                setSelected(nullptr);
        });

    if (button->state() == SelectableTextButton::State::selected)
        setSelected(button); //< Adding button in selected state replaces current selection.

    return true;
}

bool SelectableTextButtonGroup::remove(SelectableTextButton* button)
{
    if (!button || !m_buttons.remove(button))
        return false;

    button->disconnect(this);

    if (m_selectionFallback == button)
        m_selectionFallback = nullptr;

    if (m_selected == button)
    {
        m_selected = nullptr;
        emit selectionChanged(button, nullptr);
    }

    return true;
}

SelectableTextButton* SelectableTextButtonGroup::selected() const
{
    return m_selected;
}

bool SelectableTextButtonGroup::setSelected(SelectableTextButton* button)
{
    if (!button)
        button = m_selectionFallback;

    if (m_selectionChanging || button == m_selected)
        return true;

    if (button && !m_buttons.contains(button))
        return false;

    auto oldButton = m_selected;

    QnRaiiGuard guard([this]() { m_selectionChanging = false; });
    m_selectionChanging = true;

    if (oldButton && oldButton->state() == SelectableTextButton::State::selected)
        oldButton->setState(SelectableTextButton::State::unselected);

    if (button && button->state() != SelectableTextButton::State::selected)
        button->setState(SelectableTextButton::State::selected);

    m_selected = button;
    guard.finalize();

    emit selectionChanged(oldButton, button);
    return true;
}

SelectableTextButton* SelectableTextButtonGroup::selectionFallback() const
{
    return m_selectionFallback;
}

bool SelectableTextButtonGroup::setSelectionFallback(SelectableTextButton* value)
{
    if (value && !m_buttons.contains(value))
        return false;

    m_selectionFallback = value;

    if (!m_selected)
        setSelected(value);

    return true;
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
