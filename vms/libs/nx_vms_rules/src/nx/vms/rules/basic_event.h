// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QSharedPointer>

#include <nx/vms/api/rules/event_info.h>

#include "manifest.h"
#include "rules_fwd.h"

namespace nx::vms::rules {

/**
 * Base class for storing data of input events produced by event connectors.
 * Derived classes should provide Q_PROPERTY's for all significant variables
 * (event fields) of their event type.
 */
class NX_VMS_RULES_API BasicEvent: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(std::chrono::microseconds timestamp MEMBER m_timestamp)
    Q_PROPERTY(QString caption READ caption WRITE setCaption)
    Q_PROPERTY(QString description READ description WRITE setDescription)

public:
    using State = nx::vms::api::rules::EventInfo::State;

public:
    explicit BasicEvent(const nx::vms::api::rules::EventInfo& info);
    explicit BasicEvent(std::chrono::microseconds timestamp);

    QString type() const;

    virtual QString caption() const;
    virtual void setCaption(const QString& caption);
    virtual QString description() const;
    virtual void setDescription(const QString& description);

protected:
    BasicEvent() = default;

private:
    QString m_type;
    State m_state{State::none};

    std::chrono::microseconds m_timestamp;
};

} // namespace nx::vms::rules

Q_DECLARE_METATYPE(nx::vms::rules::EventPtr)
