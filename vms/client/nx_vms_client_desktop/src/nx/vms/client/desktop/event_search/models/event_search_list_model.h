// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/event_search/models/abstract_attributed_event_model.h>
#include <nx/vms/event/event_fwd.h>

class QnWorkbenchContext;

namespace nx::vms::client::desktop {

class WindowContext;

class EventSearchListModel: public core::AbstractAttributedEventModel
{
    Q_OBJECT
    using base_type = core::AbstractAttributedEventModel;

    Q_PROPERTY(nx::vms::api::EventType selectedEventType READ selectedEventType
        WRITE setSelectedEventType NOTIFY selectedEventTypeChanged)

    Q_PROPERTY(QString selectedSubType READ selectedSubType
        WRITE setSelectedSubType NOTIFY selectedSubTypeChanged)

public:
    explicit EventSearchListModel(WindowContext* context, QObject* parent = nullptr);
    virtual ~EventSearchListModel() override;

    nx::vms::api::EventType selectedEventType() const;
    void setSelectedEventType(nx::vms::api::EventType value);

    QString selectedSubType() const;
    void setSelectedSubType(const QString& value);

    // Event type list used when selectedEventType() == EventType::undefined.
    std::vector<nx::vms::api::EventType> defaultEventTypes() const;
    void setDefaultEventTypes(const std::vector<nx::vms::api::EventType>& value);

    virtual bool isConstrained() const override;
    virtual bool hasAccessRights() const override;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex& index, int role) const override;

    virtual bool requestFetch(
        const core::FetchRequest& request,
        const FetchCompletionHandler& completionHandler) override;

    virtual void clearData() override;

    virtual QnTimePeriod truncateToRelevantTimePeriod() { return {}; }
    virtual void truncateToMaximumCount() {}

signals:
    void selectedEventTypeChanged();
    void selectedSubTypeChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
