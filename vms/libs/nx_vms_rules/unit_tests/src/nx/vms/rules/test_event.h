// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <analytics/common/object_metadata.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/rules/basic_event.h>
#include <nx/vms/rules/camera_conflict_list.h>
#include <nx/vms/rules/utils/type.h>

namespace nx::vms::rules::test {

/** May be used for serialization test, should include all event prop types. */
class TestEvent: public nx::vms::rules::BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.test")

    Q_PROPERTY(QnUuid serverId MEMBER m_serverId)
    Q_PROPERTY(QnUuid cameraId MEMBER m_cameraId)

    Q_PROPERTY(int intField MEMBER m_intField)
    Q_PROPERTY(QString stringField MEMBER m_stringField)
    Q_PROPERTY(bool boolField MEMBER m_boolField)
    Q_PROPERTY(double floatField MEMBER m_floatField)

    Q_PROPERTY(nx::common::metadata::Attributes attributes MEMBER attributes)
    Q_PROPERTY(nx::vms::api::EventLevel level MEMBER level)
    Q_PROPERTY(nx::vms::api::EventReason reason MEMBER reason)
    Q_PROPERTY(nx::vms::rules::CameraConflictList conflicts MEMBER conflicts)

public:
    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = utils::type<TestEvent>(),
            .displayName = "Test event",
            .flags = {ItemFlag::instant, ItemFlag::prolonged},
        };
    }

    using BasicEvent::BasicEvent;

    QnUuid m_serverId;
    QnUuid m_cameraId;

    int m_intField{};
    QString m_stringField;
    bool m_boolField{};
    double m_floatField{};

    nx::common::metadata::Attributes attributes;
    nx::vms::api::EventLevel level = {};
    nx::vms::api::EventReason reason = {};
    nx::vms::rules::CameraConflictList conflicts;
};

class TestCameraEvent: public nx::vms::rules::BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.camera")

    Q_PROPERTY(QnUuid cameraId MEMBER m_cameraId)

public:
    using BasicEvent::BasicEvent;

    QString uniqueName() const override
    {
        return makeName(m_cameraId.toString(), BasicEvent::uniqueName());
    }

    QnUuid m_cameraId;
};

class TestTimestampDependentEvent: public nx::vms::rules::BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.time")

public:
    using BasicEvent::BasicEvent;

    QString uniqueName() const override
    {
        return makeName(QString::number(timestamp().count()), BasicEvent::uniqueName());
    }
};

} // namespace nx::vms::rules::test
