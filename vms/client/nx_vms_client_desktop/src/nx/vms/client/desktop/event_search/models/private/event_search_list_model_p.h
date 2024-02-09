// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>

#include <QtCore/QHash>
#include <QtCore/QScopedPointer>

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/utils/uuid.h>

#include "../event_search_list_model.h"
#include "abstract_async_search_list_model_p.h"

class QTimer;

namespace nx::vms::event { class StringsHelper; }

namespace nx::vms::client::desktop {

class EventSearchListModel::Private:
    public AbstractAsyncSearchListModel::Private
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel::Private;

    using EventType = nx::vms::api::EventType;
    using EventParameters = nx::vms::event::EventParameters;
    using ActionType = nx::vms::api::ActionType;
    using ActionData = nx::vms::event::ActionData;
    using ActionDataList = nx::vms::event::ActionDataList;

public:
    explicit Private(EventSearchListModel* q);
    virtual ~Private() override;

    EventType selectedEventType() const;
    void setSelectedEventType(EventType value);

    QString selectedSubType() const;
    void setSelectedSubType(const QString& value);

    const std::vector<nx::vms::api::EventType>& defaultEventTypes() const;
    void setDefaultEventTypes(const std::vector<nx::vms::api::EventType>& value);

    virtual int count() const override;
    virtual QVariant data(const QModelIndex& index, int role, bool& handled) const override;

    virtual void clearData() override;
    virtual void truncateToMaximumCount() override;
    virtual void truncateToRelevantTimePeriod() override;

protected:
    virtual rest::Handle requestPrefetch(const QnTimePeriod& period) override;
    virtual bool commitPrefetch(const QnTimePeriod& periodToCommit) override;

private:
    using GetCallback = std::function<void(bool, rest::Handle, ActionDataList&&)>;
    rest::Handle getEvents(
        const QnTimePeriod& period, GetCallback callback, Qt::SortOrder order, int limit) const;

    void fetchLive();

    template<typename Iter>
    bool commitInternal(const QnTimePeriod& periodToCommit, Iter prefetchBegin, Iter prefetchEnd,
        int position, bool handleOverlaps);

    QString title(const EventParameters& parameters) const;
    QString description(const EventParameters& parameters) const;
    static QString iconPath(const EventParameters& parameters);
    static QColor color(const EventParameters& parameters);
    static bool hasPreview(EventType eventType);

private:
    EventSearchListModel* const q;
    const QScopedPointer<nx::vms::event::StringsHelper> m_helper;

    EventType m_selectedEventType = EventType::undefinedEvent;
    QString m_selectedSubType;

    std::vector<EventType> m_defaultEventTypes{EventType::anyEvent};

    ActionDataList m_prefetch;
    std::deque<ActionData> m_data;

    QScopedPointer<QTimer> m_liveUpdateTimer;
    FetchInformation m_liveFetch;
};

} // namespace nx::vms::client::desktop
