// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QSet>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API NonUniqueNameTracker
{
public:
    QSet<QnUuid> nonUniqueNameIds() const { return m_nonUniqueNameIds; }
    bool isUnique(const QnUuid& id) const { return !m_nonUniqueNameIds.contains(id); }

    bool update(const QnUuid& id, const QString& name);
    bool remove(const QnUuid& id);

private:
    QSet<QnUuid> m_nonUniqueNameIds;
    QHash<QString, QSet<QnUuid>> m_idsByName;
    QHash<QnUuid, QString> m_nameById;
};

} // namespace nx::vms::client::desktop
