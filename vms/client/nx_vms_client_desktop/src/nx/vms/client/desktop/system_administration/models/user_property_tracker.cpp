// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_property_tracker.h"

namespace nx::vms::client::desktop {

UserPropertyTracker::UserPropertyTracker(SystemContext* systemContext):
    base_type(systemContext)
{
}

void UserPropertyTracker::addOrUpdate(const QnUserResourcePtr& user, const Properties& properties)
{
    if (properties.selected)
        m_selected.insert(user);
    else
        m_selected.remove(user);

    if (properties.disabled)
        m_disabled.insert(user);
    else
        m_disabled.remove(user);

    if (properties.selected && properties.editable)
        m_selectedEditable.insert(user);
    else
        m_selectedEditable.remove(user);

    if (properties.selected && properties.deletable)
        m_selectedDeletable.insert(user);
    else
        m_selectedDeletable.remove(user);

    if (properties.selected && properties.editable && properties.deletable)
        m_selectedModifiable.insert(user);
    else
        m_selectedModifiable.remove(user);
}

void UserPropertyTracker::remove(const QnUserResourcePtr& user)
{
    m_selected.remove(user);
    m_disabled.remove(user);
    m_selectedDeletable.remove(user);
    m_selectedEditable.remove(user);
    m_selectedModifiable.remove(user);
}

void UserPropertyTracker::clear()
{
    m_selected.clear();
    m_disabled.clear();
    m_selectedDeletable.clear();
    m_selectedEditable.clear();
    m_selectedModifiable.clear();
}

} // namespace nx::vms::client::desktop
