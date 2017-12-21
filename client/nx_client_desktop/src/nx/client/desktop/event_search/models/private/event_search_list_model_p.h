#pragma once

#include "../event_search_list_model.h"

#include <deque>

#include <QtCore/QHash>
#include <QtCore/QScopedPointer>

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/event_search/models/private/abstract_async_search_list_model_p.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/actions/abstract_action.h>

class QTimer;
class QnUuid;

namespace nx {

namespace vms { namespace event { class StringsHelper; }}

namespace client {
namespace desktop {

class EventSearchListModel::Private: public AbstractAsyncSearchListModel::Private
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel::Private;

public:
    explicit Private(EventSearchListModel* q);
    virtual ~Private() override;

    virtual void setCamera(const QnVirtualCameraResourcePtr& camera) override;

    vms::event::EventType selectedEventType() const;
    void setSelectedEventType(vms::event::EventType value);

    virtual int count() const override;
    virtual QVariant data(const QModelIndex& index, int role, bool& handled) const override;

    virtual void clear() override;

protected:
    virtual rest::Handle requestPrefetch(qint64 fromMs, qint64 toMs) override;
    virtual bool commitPrefetch(qint64 earliestTimeToCommitMs, bool& fetchedAll) override;
    virtual void clipToSelectedTimePeriod() override;
    virtual bool hasAccessRights() const override;

private:
    void periodicUpdate();
    void refreshUpdateTimer();
    void addNewlyReceivedEvents(vms::event::ActionDataList&& data);

    using GetCallback = std::function<void(bool, rest::Handle, vms::event::ActionDataList&&)>;
    rest::Handle getEvents(qint64 startMs, qint64 endMs, GetCallback callback,
        int limit = std::numeric_limits<int>::max());

    QString title(vms::event::EventType eventType) const;
    QString description(const vms::event::EventParameters& parameters) const;
    static QPixmap pixmap(const vms::event::EventParameters& parameters);
    static QColor color(vms::event::EventType eventType);
    static bool hasPreview(vms::event::EventType eventType);

private:
    EventSearchListModel* const q = nullptr;
    vms::event::EventType m_selectedEventType = vms::event::undefinedEvent;
    QScopedPointer<QTimer> m_updateTimer;
    mutable QScopedPointer<vms::event::StringsHelper> m_helper;
    qint64 m_latestTimeMs = 0;
    rest::Handle m_currentUpdateId = rest::Handle();

    vms::event::ActionDataList m_prefetch;
    std::deque<vms::event::ActionData> m_data;
    bool m_success = false;
};

} // namespace desktop
} // namespace client
} // namespace nx
