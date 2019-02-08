#pragma once

#include "../event_search_list_model.h"

#include <deque>

#include <QtCore/QHash>
#include <QtCore/QScopedPointer>

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>

#include <nx/vms/client/desktop/event_search/models/private/abstract_async_search_list_model_p.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/actions/abstract_action.h>

class QTimer;
class QnUuid;

namespace nx::vms::event { class StringsHelper; }

namespace nx::vms::client::desktop {

class EventSearchListModel::Private: public AbstractAsyncSearchListModel::Private
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel::Private;

public:
    explicit Private(EventSearchListModel* q);
    virtual ~Private() override;

    vms::api::EventType selectedEventType() const;
    void setSelectedEventType(vms::api::EventType value);

    QString selectedSubType() const;
    void setSelectedSubType(const QString& value);

    virtual int count() const override;
    virtual QVariant data(const QModelIndex& index, int role, bool& handled) const override;

    virtual void clearData() override;
    virtual void truncateToMaximumCount() override;
    virtual void truncateToRelevantTimePeriod() override;

protected:
    virtual rest::Handle requestPrefetch(const QnTimePeriod& period) override;
    virtual bool commitPrefetch(const QnTimePeriod& periodToCommit) override;

private:
    using GetCallback = std::function<void(bool, rest::Handle, vms::event::ActionDataList&&)>;
    rest::Handle getEvents(
        const QnTimePeriod& period, GetCallback callback, Qt::SortOrder order, int limit) const;

    void fetchLive();

    template<typename Iter>
    bool commitInternal(const QnTimePeriod& periodToCommit, Iter prefetchBegin, Iter prefetchEnd,
        int position, bool handleOverlaps);

    QString title(vms::api::EventType eventType) const;
    QString description(const vms::event::EventParameters& parameters) const;
    static QPixmap pixmap(const vms::event::EventParameters& parameters);
    static QColor color(const vms::event::EventParameters& parameters);
    static bool hasPreview(vms::api::EventType eventType);

private:
    EventSearchListModel* const q;
    const QScopedPointer<vms::event::StringsHelper> m_helper;

    vms::api::EventType m_selectedEventType = vms::api::undefinedEvent;
    QString m_selectedSubType;

    vms::event::ActionDataList m_prefetch;
    std::deque<vms::event::ActionData> m_data;

    QScopedPointer<QTimer> m_liveUpdateTimer;
    FetchInformation m_liveFetch;
};

} // namespace nx::vms::client::desktop
