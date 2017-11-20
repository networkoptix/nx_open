#pragma once

#include "../notification_list_model.h"

#include <QtCore/QHash>
#include <QtCore/QSharedPointer>
#include <QtCore/QScopedPointer>

#include <nx/vms/event/event_fwd.h>

class AudioPlayer;

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

private:
    void addNotification(const vms::event::AbstractActionPtr& action);
    void removeNotification(const vms::event::AbstractActionPtr& action);

    void setupAcknowledgeAction(EventData& eventData,
        const QnVirtualCameraResourcePtr& camera,
        const nx::vms::event::AbstractActionPtr& action);

    QPixmap pixmapForAction(const vms::event::AbstractActionPtr& action,
        const QColor& color = QColor()) const;

private:
    NotificationListModel* const q = nullptr;
    QScopedPointer<vms::event::StringsHelper> m_helper;
    QHash<QnUuid/*ruleId*/, QHash<QnResourcePtr, QSet<QnUuid /*itemId*/>>> m_uuidHashes;
    QMultiHash<QString, QnUuid> m_itemsByLoadingSound;
    QHash<QnUuid, QSharedPointer<AudioPlayer>> m_players;
};

} // namespace
} // namespace client
} // namespace nx
