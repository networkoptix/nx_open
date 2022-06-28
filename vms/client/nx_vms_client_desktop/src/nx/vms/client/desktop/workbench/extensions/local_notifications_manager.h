// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <variant>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtGui/QIcon>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/common/utils/command_action.h>
#include <nx/vms/client/desktop/common/utils/progress_state.h>
#include <ui/common/notification_levels.h>

namespace nx::vms::client::desktop::workbench {

/** A manager for Client-local notifications. */
class LocalNotificationsManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    LocalNotificationsManager(QObject* parent = nullptr);
    virtual ~LocalNotificationsManager() override = default;

    QList<QnUuid> notifications() const;

    QnUuid add(
        const QString& title, const QString& description = QString(), bool cancellable = false);
    QnUuid addProgress(
        const QString& title, const QString& description = QString(), bool cancellable = false);
    void remove(const QnUuid& notificationId);

    QString title(const QnUuid& notificationId) const;
    void setTitle(const QnUuid& notificationId, const QString& value);

    QString description(const QnUuid& notificationId) const;
    void setDescription(const QnUuid& notificationId, const QString& value);

    QPixmap icon(const QnUuid& notificationId) const;
    void setIcon(const QnUuid& notificationId, const QPixmap& value);

    std::optional<ProgressState> progress(const QnUuid& notificationId) const;
    void setProgress(const QnUuid& notificationId, std::optional<ProgressState> value);

    QString progressFormat(const QnUuid& notificationId) const;
    void setProgressFormat(const QnUuid& notificationId, const QString& value);

    bool isCancellable(const QnUuid& notificationId) const;
    void setCancellable(const QnUuid& notificationId, bool value);

    CommandActionPtr action(const QnUuid& notificationId) const;
    void setAction(const QnUuid& notificationId, CommandActionPtr value);

    QnNotificationLevel::Value level(const QnUuid& notificationId) const;
    void setLevel(const QnUuid& notificationId, QnNotificationLevel::Value level);

    QString additionalText(const QnUuid& notificationId) const;
    void setAdditionalText(const QnUuid& notificationId, QString additionalText);

    // Common UI should call this function to request activity cancellation.
    // Just emits cancelRequested after validity check.
    void cancel(const QnUuid& notificationId);

    // Common UI should call this function to request dedicated UI to pop.
    // Just emits interactionRequested after validity check.
    void interact(const QnUuid& notificationId);

signals:
    void added(const QnUuid& notificationId);
    void removed(const QnUuid& notificationId);
    void cancelRequested(const QnUuid& notificationId);
    void interactionRequested(const QnUuid& notificationId);
    void progressChanged(const QnUuid& notificationId, std::optional<ProgressState> progress);
    void progressFormatChanged(const QnUuid& notificationId, const QString& format);
    void cancellableChanged(const QnUuid& notificationId, bool isCancellable);
    void titleChanged(const QnUuid& notificationId, const QString& title);
    void descriptionChanged(const QnUuid& notificationId, const QString& description);
    void iconChanged(const QnUuid& notificationId, const QIcon& icon);
    void actionChanged(const QnUuid& notificationId, CommandActionPtr action);
    void levelChanged(const QnUuid& notificationId, QnNotificationLevel::Value level);
    void additionalTextChanged(const QnUuid& notificationId, const QString& additionalText);

private:
    struct State
    {
        QString title;
        QString description;
        bool cancellable = false;
        std::optional<ProgressState> progress;
        CommandActionPtr action = nullptr;
        QString format;
        QPixmap icon;
        QnNotificationLevel::Value level = QnNotificationLevel::Value::NoNotification;
        QString additionalText;

        State() = default;
        State(
            const QString& title,
            const QString& description,
            bool cancellable,
            std::optional<ProgressState> progress)
            :
            title(title),
            description(description),
            cancellable(cancellable),
            progress(progress)
        {
        }
    };

    QnUuid add(State state);

    QList<QnUuid> m_ids; //< Sorted by insertion order.
    QHash<QnUuid, State> m_lookup;
    mutable Mutex m_mutex;
};

} // namespace nx::vms::client::desktop::workbench
