// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QSet>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API NonUniqueNameTracker
{
public:
    QSet<nx::Uuid> nonUniqueNameIds() const { return m_nonUniqueNameIds; }
    bool isUnique(const nx::Uuid& id) const { return !m_nonUniqueNameIds.contains(id); }

    bool update(const nx::Uuid& id, const QString& name);
    bool remove(const nx::Uuid& id);

    QSet<nx::Uuid> idsByName(const QString& name) const;

private:
    QSet<nx::Uuid> m_nonUniqueNameIds;
    QHash<QString, QSet<nx::Uuid>> m_idsByName;
    QHash<nx::Uuid, QString> m_nameById;
};

} // namespace nx::vms::client::desktop
