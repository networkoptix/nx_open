// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/vms/client/desktop/event_search/models/abstract_async_search_list_model.h>
#include <nx/vms/event/event_fwd.h>

namespace nx::vms::client::desktop {

class EventSearchListModel: public AbstractAsyncSearchListModel
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel;

    Q_PROPERTY(nx::vms::api::EventType selectedEventType READ selectedEventType
        WRITE setSelectedEventType NOTIFY selectedEventTypeChanged)

    Q_PROPERTY(QString selectedSubType READ selectedSubType
        WRITE setSelectedSubType NOTIFY selectedSubTypeChanged)

public:
    explicit EventSearchListModel(WindowContext* context, QObject* parent = nullptr);
    virtual ~EventSearchListModel() override = default;

    nx::vms::api::EventType selectedEventType() const;
    void setSelectedEventType(nx::vms::api::EventType value);

    QString selectedSubType() const;
    void setSelectedSubType(const QString& value);

    // Event type list used when selectedEventType() == EventType::undefined.
    std::vector<nx::vms::api::EventType> defaultEventTypes() const;
    void setDefaultEventTypes(const std::vector<nx::vms::api::EventType>& value);

    virtual bool isConstrained() const override;
    virtual bool hasAccessRights() const override;

signals:
    void selectedEventTypeChanged();
    void selectedSubTypeChanged();

private:
    class Private;
    Private* const d;
};

} // namespace nx::vms::client::desktop
