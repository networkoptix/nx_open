// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>

#include <QtCore/QScopedPointer>

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/api/rules/event_log.h>
#include <nx/vms/rules/aggregated_event.h>

#include "../vms_event_search_list_model.h"
#include "abstract_async_search_list_model_p.h"
#include "event_model_data.h"

class QTimer;

namespace nx::vms::client::desktop {

class VmsEventSearchListModel::Private:
    public AbstractAsyncSearchListModel::Private
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel::Private;

    using EventLogRecordList = nx::vms::api::rules::EventLogRecordList;
    using EventPtr = nx::vms::rules::EventPtr;

public:
    explicit Private(VmsEventSearchListModel* q);
    virtual ~Private() override;

    QString selectedEventType() const;
    void setSelectedEventType(const QString& value);

    QString selectedSubType() const;
    void setSelectedSubType(const QString& value);

    const QStringList& defaultEventTypes() const;
    void setDefaultEventTypes(const QStringList& value);

    virtual int count() const override;
    virtual QVariant data(const QModelIndex& index, int role, bool& handled) const override;

    virtual void clearData() override;
    virtual void truncateToMaximumCount() override;
    virtual void truncateToRelevantTimePeriod() override;

protected:
    virtual rest::Handle requestPrefetch(const QnTimePeriod& period) override;
    virtual bool commitPrefetch(const QnTimePeriod& periodToCommit) override;

private:
    using GetCallback = std::function<void(bool, rest::Handle, EventLogRecordList&&)>;
    rest::Handle getEvents(
        const QnTimePeriod& period, GetCallback callback, Qt::SortOrder order, int limit) const;

    void fetchLive();

    template<typename Iter>
    bool commitInternal(const QnTimePeriod& periodToCommit, Iter prefetchBegin, Iter prefetchEnd,
        int position, bool handleOverlaps);

    QString description(const QVariantMap& details) const;
    QString iconPath(const EventPtr& event, const QVariantMap& details) const;
    static QColor color(const QVariantMap& details);
    static bool hasPreview(const EventPtr& event);

private:
    VmsEventSearchListModel* const q;
    const QScopedPointer<nx::vms::event::StringsHelper> m_helper;

    QString m_selectedEventType;
    QString m_selectedSubType;

    QStringList m_defaultEventTypes;

    EventLogRecordList m_prefetch;
    std::deque<EventModelData> m_data;

    QScopedPointer<QTimer> m_liveUpdateTimer;
    FetchInformation m_liveFetch;
};

} // namespace nx::vms::client::desktop
