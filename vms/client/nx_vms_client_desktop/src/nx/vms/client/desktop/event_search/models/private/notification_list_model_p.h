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

private:
    void onNotificationAction(
        const QSharedPointer<nx::vms::rules::NotificationAction>& action,
        QString cloudSystemId = {});
    void addNotification(const vms::event::AbstractActionPtr& action);
    void removeNotification(const vms::event::AbstractActionPtr& action);

    void onRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last);
    void onCloudCrossSystemStatusChanged(const QString& systemId);
    void updateCloudItems(const QString& systemId);

    void setupAcknowledgeAction(EventData& eventData, const QnUuid& cameraId,
        const nx::vms::event::AbstractActionPtr& action);

    void setupIntercomAcknowledgeAction(
        EventData& eventData,
        const QnUuid& cameraId,
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
        const QString& cloudSystemId,
        const QColor& color) const;

    void setupClientAction(
        const nx::vms::rules::NotificationAction* action,
        EventData& eventData) const;

    void truncateToMaximumCount();

private:
    NotificationListModel* const q = nullptr;
    int m_maximumCount = NotificationListModel::kDefaultMaximumCount;
    QScopedPointer<vms::event::StringsHelper> m_helper;

    QHash<QnUuid/*ruleId*/, QHash<QnResourcePtr, QSet<QnUuid /*itemId*/>>> m_uuidHashes;
    QMultiHash<QString, QnUuid> m_itemsByLoadingSound;
    QHash<QnUuid, QSharedPointer<AudioPlayer>> m_players;
    QMultiHash<QString /*system id*/, QnUuid /*item id*/> m_itemsByCloudSystem;
};

} // namespace nx::vms::client::desktop
