#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>

#include <nx/vms/client/desktop/common/utils/command_action.h>

namespace nx::vms::client::desktop {

/**
* A class that maintains a list of global operations currently in progress.
* Acts as a mediator between global asynchonous operations and UI displaying their progess.
*/
class WorkbenchProgressManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    WorkbenchProgressManager(QObject* parent = nullptr);
    virtual ~WorkbenchProgressManager() override = default;

    QList<QnUuid> activities() const;

    QnUuid add(const QString& title, const QString& description = QString(), bool cancellable = false);
    void remove(const QnUuid& activityId);

    QString title(const QnUuid& activityId) const;
    void setTitle(const QnUuid& activityId, const QString& value);

    QString description(const QnUuid& activityId) const;
    void setDescription(const QnUuid& activityId, const QString& value);

    qreal progress(const QnUuid& activityId) const;
    void setProgress(const QnUuid& activityId, qreal value);

    QString progressFormat(const QnUuid& activityId) const;
    void setProgressFormat(const QnUuid& activityId, const QString& value);

    static constexpr qreal kIndefiniteProgressValue = -1;
    static constexpr qreal kCompletedProgressValue = -2;
    static constexpr qreal kFailedProgressValue = -3;

    bool isCancellable(const QnUuid& activityId) const;
    void setCancellable(const QnUuid& activityId, bool value);

    CommandActionPtr action(const QnUuid& activityId) const;
    void setAction(const QnUuid& activityId, CommandActionPtr value);

    // Common UI should call this function to request activity cancellation.
    // Just emits cancelRequested after validity check.
    void cancel(const QnUuid& activityId);

    // Common UI should call this function to request dedicated UI to pop.
    // Just emits interactionRequested after validity check.
    void interact(const QnUuid& activityId);

signals:
    void added(const QnUuid& activityId);
    void removed(const QnUuid& activityId);
    void cancelRequested(const QnUuid& activityId);
    void interactionRequested(const QnUuid& activityId);
    void progressChanged(const QnUuid& activityId, qreal progress);
    void progressFormatChanged(const QnUuid& activityId, const QString& format);
    void cancellableChanged(const QnUuid& activityId, bool isCancellable);
    void titleChanged(const QnUuid& activityId, const QString& title);
    void descriptionChanged(const QnUuid& activityId, const QString& description);
    void actionChanged(const QnUuid& activityId, CommandActionPtr action);

private:
    struct State
    {
        QString title;
        QString description;
        bool cancellable = false;
        qreal progress = 0.0;
        CommandActionPtr action = nullptr;
        QString format;

        State() = default;
        State(const QString& title, const QString& description, bool cancellable, qreal progress):
            title(title),
            description(description),
            cancellable(cancellable),
            progress(progress)
        {
        }
    };

    QList<QnUuid> m_ids; //< Sorted by insertion order.
    QHash<QnUuid, State> m_lookup;
    mutable QnMutex m_mutex;
};

} // namespace nx::vms::client::desktop
