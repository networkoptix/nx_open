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

    QList<nx::Uuid> notifications() const;

    nx::Uuid add(
        const QString& title, const QString& description = QString(), bool cancellable = false);
    nx::Uuid addProgress(
        const QString& title, const QString& description = QString(), bool cancellable = false);
    void remove(const nx::Uuid& notificationId);

    QString title(const nx::Uuid& notificationId) const;
    void setTitle(const nx::Uuid& notificationId, const QString& value);

    QString description(const nx::Uuid& notificationId) const;
    void setDescription(const nx::Uuid& notificationId, const QString& value);

    QPixmap icon(const nx::Uuid& notificationId) const;
    void setIcon(const nx::Uuid& notificationId, const QPixmap& value);

    std::optional<ProgressState> progress(const nx::Uuid& notificationId) const;
    void setProgress(const nx::Uuid& notificationId, std::optional<ProgressState> value);

    QString progressFormat(const nx::Uuid& notificationId) const;
    void setProgressFormat(const nx::Uuid& notificationId, const QString& value);

    bool isCancellable(const nx::Uuid& notificationId) const;
    void setCancellable(const nx::Uuid& notificationId, bool value);

    CommandActionPtr action(const nx::Uuid& notificationId) const;
    void setAction(const nx::Uuid& notificationId, CommandActionPtr value);

    CommandActionPtr additionalAction(const nx::Uuid& notificationId) const;
    void setAdditionalAction(const nx::Uuid& notificationId, CommandActionPtr value);

    QnNotificationLevel::Value level(const nx::Uuid& notificationId) const;
    void setLevel(const nx::Uuid& notificationId, QnNotificationLevel::Value level);

    QString additionalText(const nx::Uuid& notificationId) const;
    void setAdditionalText(const nx::Uuid& notificationId, QString additionalText);

    QString tooltip(const nx::Uuid& notificationId) const;
    void setTooltip(const nx::Uuid& notificationId, QString tooltip);

    // Common UI should call this function to request activity cancellation.
    // Just emits cancelRequested after validity check.
    void cancel(const nx::Uuid& notificationId);

    // Common UI should call this function to request dedicated UI to pop.
    // Just emits interactionRequested after validity check.
    void interact(const nx::Uuid& notificationId);

signals:
    void added(const nx::Uuid& notificationId);
    void removed(const nx::Uuid& notificationId);
    void cancelRequested(const nx::Uuid& notificationId);
    void interactionRequested(const nx::Uuid& notificationId);
    void progressChanged(const nx::Uuid& notificationId, std::optional<ProgressState> progress);
    void progressFormatChanged(const nx::Uuid& notificationId, const QString& format);
    void cancellableChanged(const nx::Uuid& notificationId, bool isCancellable);
    void titleChanged(const nx::Uuid& notificationId, const QString& title);
    void descriptionChanged(const nx::Uuid& notificationId, const QString& description);
    void iconChanged(const nx::Uuid& notificationId, const QIcon& icon);
    void actionChanged(const nx::Uuid& notificationId, CommandActionPtr action);
    void levelChanged(const nx::Uuid& notificationId, QnNotificationLevel::Value level);
    void additionalTextChanged(const nx::Uuid& notificationId, const QString& additionalText);
    void tooltipChanged(const nx::Uuid& notificationId, const QString& tooltip);
    void additionalActionChanged(const nx::Uuid& notificationId, CommandActionPtr action);

private:
    struct State
    {
        QString title;
        QString description;
        bool cancellable = false;
        std::optional<ProgressState> progress;
        CommandActionPtr action = nullptr;
        CommandActionPtr additionalAction = nullptr;
        QString format;
        QPixmap icon;
        QnNotificationLevel::Value level = QnNotificationLevel::Value::NoNotification;
        QString additionalText;
        QString tooltip;

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

    nx::Uuid add(State state);

    QList<nx::Uuid> m_ids; //< Sorted by insertion order.
    QHash<nx::Uuid, State> m_lookup;
    mutable Mutex m_mutex;
};

} // namespace nx::vms::client::desktop::workbench
