// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../notification_list_model.h"

#include <QtCore/QHash>
#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>

#include <nx/vms/event/event_fwd.h>

class AudioPlayer;

namespace nx::vms::rules { class NotificationAction; }

namespace nx::vms::client::desktop {

class NotificationListModel::Private:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Private(NotificationListModel* q);
    virtual ~Private() override;

    int maximumCount() const;
    void setMaximumCount(int value);

    using ExtraData = QPair<QnUuid /*ruleId*/, QnResourcePtr>;
    static ExtraData extraData(const EventData& event);

private:
    void onNotificationAction(const QSharedPointer<nx::vms::rules::NotificationAction>& action,
        QString cloudSystemId = {});
    void addNotification(const vms::event::AbstractActionPtr& action);
    void removeNotification(const vms::event::AbstractActionPtr& action);

    void setupAcknowledgeAction(EventData& eventData, const QnUuid& cameraId,
        const nx::vms::event::AbstractActionPtr& action);

    QString caption(const nx::vms::event::EventParameters& parameters,
        const QnVirtualCameraResourcePtr& camera) const;
    QString description(const nx::vms::event::EventParameters& parameters) const;
    QString tooltip(const vms::event::AbstractActionPtr& action) const;

    QString getPoeOverBudgetDescription(const nx::vms::event::EventParameters& parameters) const;

    QPixmap pixmapForAction(
        const vms::event::AbstractActionPtr& action,
        const QColor& color) const;
    QPixmap pixmapForAction(
        const nx::vms::rules::NotificationAction* action,
        const QColor& color) const;

    void truncateToMaximumCount();

private:
    NotificationListModel* const q = nullptr;
    QScopedPointer<vms::event::StringsHelper> m_helper;
    QHash<QnUuid/*ruleId*/, QHash<QnResourcePtr, QSet<QnUuid /*itemId*/>>> m_uuidHashes;
    QMultiHash<QString, QnUuid> m_itemsByLoadingSound;
    QHash<QnUuid, QSharedPointer<AudioPlayer>> m_players;
    int m_maximumCount = NotificationListModel::kDefaultMaximumCount;
};

} // namespace nx::vms::client::desktop
