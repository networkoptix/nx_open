// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_action.h>

#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API NotificationAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.desktopNotification")

    Q_PROPERTY(QnUuid id READ id WRITE setId)
    FIELD(QString, caption, setCaption)
    FIELD(QString, description, setDescription)
    FIELD(QString, tooltip, setTooltip)
    FIELD(nx::vms::rules::UuidSelection, users, setUsers)
    FIELD(std::chrono::seconds, interval, setInterval)
    FIELD(bool, acknowledge, setAcknowledge)
    FIELD(QnUuid, cameraId, setCameraId)

public:
    NotificationAction();
    static const ItemDescriptor& manifest();
    QnUuid id() const;
    void setId(const QnUuid& id);

private:
    QnUuid m_id;
};

} // namespace nx::vms::rules

Q_DECLARE_METATYPE(QSharedPointer<nx::vms::rules::NotificationAction>)
