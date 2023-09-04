// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <analytics/common/object_metadata.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/rules/basic_event.h>
#include <nx/vms/rules/camera_conflict_list.h>
#include <nx/vms/rules/utils/event_details.h>
#include <nx/vms/rules/utils/type.h>

namespace nx::vms::rules::test {

// Simplest event without permissions.
class SimpleEvent: public nx::vms::rules::BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.test")

    Q_PROPERTY(QnUuid cameraId MEMBER m_cameraId)
    Q_PROPERTY(QnUuidList deviceIds MEMBER m_deviceIds)
public:
    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = utils::type<SimpleEvent>(),
            .displayName = "Test event",
            .flags = {ItemFlag::instant, ItemFlag::prolonged},
        };
    }

    virtual QString resourceKey() const override
    {
        return m_cameraId.toSimpleString();
    }

    using BasicEvent::BasicEvent;
    QnUuid m_cameraId;
    QnUuidList m_deviceIds;
};

class TestEvent: public nx::vms::rules::BasicEvent
{
    using base_type = BasicEvent;
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.test.permissions")

    Q_PROPERTY(QnUuid serverId MEMBER m_serverId)
    Q_PROPERTY(QnUuid cameraId MEMBER m_cameraId)
    Q_PROPERTY(QnUuidList deviceIds MEMBER m_deviceIds)

    Q_PROPERTY(int intField MEMBER m_intField)
    Q_PROPERTY(QString text MEMBER m_text)
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
            .displayName = "Test event with permissions",
            .flags = {ItemFlag::instant, ItemFlag::prolonged},
            .permissions = {
                .globalPermission = nx::vms::api::GlobalPermission::viewLogs,
                .resourcePermissions = {
                    {"cameraId", Qn::ViewContentPermission},
                    {"deviceIds", Qn::ViewContentPermission},
                }
            },
        };
    }

    using BasicEvent::BasicEvent;

    virtual QString uniqueName() const override
    {
        return utils::makeName(BasicEvent::uniqueName(), m_cameraId.toSimpleString());
    }

    virtual QString resourceKey() const override
    {
        return m_cameraId.toSimpleString();
    }

    virtual QString cacheKey() const override
    {
        return m_cacheKey;
    }

    void setCacheKey(const QString& cacheKey)
    {
        m_cacheKey = cacheKey;
    }

    virtual QVariantMap details(common::SystemContext* context) const override
    {
        auto result = base_type::details(context);
        result[utils::kSourceNameDetailName] = "Test resource";
        nx::vms::rules::utils::insertLevel(result, nx::vms::event::Level::none);
        nx::vms::rules::utils::insertIcon(result, nx::vms::rules::Icon::alert);
        result[nx::vms::rules::utils::kCustomIconDetailName] = "test";
        nx::vms::rules::utils::insertClientAction(result, nx::vms::rules::ClientAction::none);
        result[nx::vms::rules::utils::kUrlDetailName] = "http://localhost";
        result[utils::kDetailingDetailName] = QStringList{"line 1", "line 2"};

        return result;
    }

    QnUuid m_serverId;
    QnUuid m_cameraId;
    QnUuidList m_deviceIds;

    int m_intField{};
    QString m_text;
    bool m_boolField{};
    double m_floatField{};

    nx::common::metadata::Attributes attributes;
    nx::vms::api::EventLevel level = {};
    nx::vms::api::EventReason reason = {};
    nx::vms::rules::CameraConflictList conflicts;

    QString m_cacheKey;
};

using SimpleEventPtr = QSharedPointer<SimpleEvent>;
using TestEventPtr = QSharedPointer<TestEvent>;

} // namespace nx::vms::rules::test
