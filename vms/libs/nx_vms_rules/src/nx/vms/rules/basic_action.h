// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

#include <chrono>

#include <nx/vms/api/rules/event_info.h>

namespace nx::vms::rules {

enum class ActionType
{
    instant = 0,
    actAtStart = 1 << 0,
    actAtEnd = 1 << 1,
    prolonged = actAtStart | actAtEnd
};

/**
 * Base class for storing data of output actions produced by action builders.
 * Derived classes should provide Q_PROPERTY's for all significant variables
 * (action fields) that are used to execute their type of action.
 */
class NX_VMS_RULES_API BasicAction: public QObject
{
    Q_OBJECT

public:
    QString type() const;

protected:
    BasicAction(const QString &type);

private:
    QString m_type;
};

using ActionPtr = QSharedPointer<BasicAction>;

class NX_VMS_RULES_API NotificationAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT

    Q_PROPERTY(QString caption READ caption WRITE setCaption)
    Q_PROPERTY(QString description READ description WRITE setDescription)

public:
    NotificationAction(): BasicAction("nx.showNotification"){}

    QString caption() const;
    void setCaption(const QString& caption);

    QString description() const;
    void setDescription(const QString& caption);

private:
    QString m_caption;
    QString m_description;
};

} // namespace nx::vms::rules
