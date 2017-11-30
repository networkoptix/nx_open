#pragma once

#include "../event_search_list_model.h"

#include <deque>
#include <limits>

#include <QtCore/QHash>
#include <QtCore/QScopedPointer>

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>

#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/actions/abstract_action.h>

class QTimer;
class QnUuid;

namespace nx {

namespace vms { namespace event { class StringsHelper; }}

namespace client {
namespace desktop {

class EventSearchListModel::Private: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(EventSearchListModel* q);
    virtual ~Private() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    vms::event::EventType selectedEventType() const;
    void setSelectedEventType(vms::event::EventType value);

    int count() const;
    const vms::event::ActionData& getEvent(int index) const;

    void clear();

    bool canFetchMore() const;
    bool prefetch(PrefetchCompletionHandler completionHandler);
    void commitPrefetch(qint64 latestStartTimeMs);

    QString title(vms::event::EventType eventType) const;
    QString description(const vms::event::EventParameters& parameters) const;
    static QPixmap pixmap(const vms::event::EventParameters& parameters);
    static QColor color(vms::event::EventType eventType);
    static bool hasPreview(vms::event::EventType eventType);

private:
    void periodicUpdate();
    void addNewlyReceivedEvents(vms::event::ActionDataList&& data);

    using GetCallback = std::function<void(bool, rest::Handle, vms::event::ActionDataList&&)>;
    rest::Handle getEvents(qint64 startMs, qint64 endMs, GetCallback callback,
        int limit = std::numeric_limits<int>::max());

private:
    EventSearchListModel* const q = nullptr;
    QnVirtualCameraResourcePtr m_camera;
    vms::event::EventType m_selectedEventType = vms::event::undefinedEvent;
    QScopedPointer<QTimer> m_updateTimer;
    QScopedPointer<vms::event::StringsHelper> m_helper;
    qint64 m_latestTimeMs = 0;
    qint64 m_earliestTimeMs = std::numeric_limits<qint64>::max();
    rest::Handle m_fetchInProgress = rest::Handle();
    rest::Handle m_updateInProgress = rest::Handle();
    bool m_fetchedAll = false;
    bool m_success = true;

    vms::event::ActionDataList m_prefetch;
    std::deque<vms::event::ActionData> m_data;
};

} // namespace
} // namespace client
} // namespace nx
