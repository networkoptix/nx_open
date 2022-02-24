// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

#include <chrono>

#include <nx/vms/api/rules/event_info.h>

#include "field_types.h"
#include "manifest.h"

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
    BasicAction() = default;

private:
    //QString m_type;
};

using ActionPtr = QSharedPointer<BasicAction>;

} // namespace nx::vms::rules
