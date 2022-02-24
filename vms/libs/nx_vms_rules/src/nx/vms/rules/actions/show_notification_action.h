// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_action.h>

#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API NotificationAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.desktopNotification")

    FIELD(UuidSelection, users, setUsers)
    FIELD(int, interval, setInterval)
    FIELD(bool, acknowlede, setAcknowledge)
    Q_PROPERTY(QString caption READ caption WRITE setCaption)
    Q_PROPERTY(QString description READ description WRITE setDescription)

public:
    QString caption() const;
    void setCaption(const QString& caption);

    QString description() const;
    void setDescription(const QString& caption);

private:
    QString m_caption;
    QString m_description;
};

} // namespace nx::vms::rules
