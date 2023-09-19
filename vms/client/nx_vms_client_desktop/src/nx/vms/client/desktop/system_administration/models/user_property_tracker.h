// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

/** Tracks users with specific properties and their combinations. */
class UserPropertyTracker: public SystemContextAware
{
    using base_type = SystemContextAware;

public:
    struct Properties
    {
        bool selected = false;
        bool disabled = false;
        bool editable = false;
        bool deletable = false;
    };

public:
    UserPropertyTracker(SystemContext* systemContext);

    void addOrUpdate(const QnUserResourcePtr& user, const Properties& properties);
    void remove(const QnUserResourcePtr& user);
    void clear();

    qsizetype disabledCount() const { return m_disabled.count(); }

    QSet<QnUserResourcePtr> selected() const { return m_selected; }
    QSet<QnUserResourcePtr> selectedEditable() const { return m_selectedEditable; }
    QSet<QnUserResourcePtr> selectedDeletable() const { return m_selectedDeletable; }

    bool hasSelectedModifiable() const { return !m_selectedModifiable.isEmpty(); }

private:
    QSet<QnUserResourcePtr> m_selected;
    QSet<QnUserResourcePtr> m_disabled;
    QSet<QnUserResourcePtr> m_selectedEditable;
    QSet<QnUserResourcePtr> m_selectedDeletable;

    QSet<QnUserResourcePtr> m_selectedModifiable;
};

} // namespace nx::vms::client::desktop
