#pragma once

#include "../notification_list_model.h"

#include <nx/vms/event/event_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class NotificationListModel::Private:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(NotificationListModel* q);
    virtual ~Private() override;

    using ExtraData = QPair<QnUuid /*ruleId*/, QnResourcePtr>;
    static ExtraData extraData(const EventData& event);

    void beforeRemove(const EventData& event);

private:
    void addNotification(const vms::event::AbstractActionPtr& action);
    void removeNotification(const vms::event::AbstractActionPtr& action);

    void setupAcknowledgeAction(EventData& eventData,
        const QnVirtualCameraResourcePtr& camera,
        const nx::vms::event::AbstractActionPtr& action);

    QPixmap pixmapForAction(const vms::event::AbstractActionPtr& action) const;

private:
    NotificationListModel* const q = nullptr;
    QScopedPointer<vms::event::StringsHelper> m_helper;
    QHash<QnUuid/*ruleId*/, QHash<QnResourcePtr, QnUuid /*itemId*/>> m_uuidHashes;
};

} // namespace
} // namespace client
} // namespace nx
