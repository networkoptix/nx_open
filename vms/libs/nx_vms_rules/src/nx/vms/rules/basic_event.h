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

    Q_PROPERTY(EventTimestamp timestamp MEMBER m_timestamp)
    Q_PROPERTY(QString type READ type)

public:
    static constexpr auto kType = "type";

    using State = nx::vms::api::rules::EventInfo::State;

public:
    explicit BasicEvent(const nx::vms::api::rules::EventInfo& info);
    explicit BasicEvent(EventTimestamp timestamp);

    QString type() const;

    /** List of event field names. */
    QStringList fields() const;

protected:
    BasicEvent();

private:
    QString m_type;
    State m_state;

    EventTimestamp m_timestamp;
};

// TODO: #sapa Unify with event and field. Choose name between type & metatype.
template<class T>
QString eventType()
{
    const auto& meta = T::staticMetaObject;
    int idx = meta.indexOfClassInfo(BasicEvent::kType);

    return (idx < 0) ? QString() : meta.classInfo(idx).value();
}

} // namespace nx::vms::rules

Q_DECLARE_METATYPE(nx::vms::rules::EventPtr)
