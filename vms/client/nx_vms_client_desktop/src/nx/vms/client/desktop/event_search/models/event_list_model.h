// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_event_list_model.h"

#include <chrono>

#include <QtGui/QPixmap>

#include <analytics/common/object_metadata.h>
#include <core/resource/resource_fwd.h>
#include <ui/common/notification_levels.h>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/common/utils/command_action.h>

namespace nx::vms::client::desktop {

class EventListModel: public AbstractEventListModel
{
    Q_OBJECT
    using base_type = AbstractEventListModel;

public:
    struct EventData
    {
        QnUuid id;
        QString title;
        QString description;
        QString toolTip;
        std::chrono::microseconds timestamp{0};
        QPixmap icon;
        QColor titleColor;
        bool removable = false;
        std::chrono::milliseconds lifetime{0};
        int helpId = -1;
        QnNotificationLevel::Value level = QnNotificationLevel::Value::NoNotification;
        std::chrono::microseconds previewTime{0}; //< The latest thumbnail's used if previewTime <= 0.
        QnUuid ruleId;
        bool forcePreviewLoader = false; //< Display loader on tile preview.

        // Resource data.
        QString cloudSystemId; //< The field is filled in only for events received from cloud.
        QnVirtualCameraResourcePtr previewCamera;
        QnVirtualCameraResourceList cameras;
        QnResourcePtr source;
        QString sourceName;

        // Client action data.
        ui::action::IDType actionId = ui::action::NoAction;
        ui::action::Parameters actionParameters;
        CommandActionPtr extraAction;

        // Analytics data.
        QnUuid objectTrackId;
        analytics::AttributeList attributes;
    };

public:
    explicit EventListModel(QnWorkbenchContext* context, QObject* parent = nullptr);
    virtual ~EventListModel() override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

    void clear();

protected:
    enum class Position
    {
        front,
        back
    };

    bool addEvent(const EventData& event, Position where = Position::front);
    bool updateEvent(const EventData& event);
    bool updateEvent(QnUuid id);
    bool removeEvent(const QnUuid& id);

    virtual bool defaultAction(const QModelIndex& index) override;

    QModelIndex indexOf(const QnUuid& id) const;
    EventData getEvent(int row) const;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
