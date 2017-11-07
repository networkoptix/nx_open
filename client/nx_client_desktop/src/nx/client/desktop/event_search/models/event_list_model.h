#pragma once

#include <QtCore/QScopedPointer>
#include <QtCore/QAbstractListModel>
#include <QtGui/QPixmap>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

#include <nx/client/desktop/ui/actions/action.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <nx/client/desktop/common/utils/command_action.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace client {
namespace desktop {

class EventListModel:
    public ScopedModelOperations<QAbstractListModel>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = ScopedModelOperations<QAbstractListModel>;

public:
    struct EventData
    {
        QnUuid id;
        QString title;
        QString description;
        QString toolTip;
        qint64 timestampMs = 0;
        QPixmap icon;
        QColor titleColor;
        bool removable = false;
        int helpId = -1;
        QnVirtualCameraResourcePtr previewCamera;
        QnVirtualCameraResourceList cameras;
        qint64 previewTimeMs = 0; //< The latest thumbnail is used if previewTimeMs <= 0.
        ui::action::IDType actionId = ui::action::NoAction;
        ui::action::Parameters actionParameters;
        CommandActionPtr extraAction;
        QVariant extraData;
    };

public:
    explicit EventListModel(QObject* parent = nullptr);
    virtual ~EventListModel() override;

    virtual int rowCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void defaultAction(const QnUuid& id);
    void closeAction(const QnUuid& id);
    void linkAction(const QnUuid& id, const QString& link);

protected:
    bool addEvent(const EventData& event);
    bool updateEvent(const EventData& event);
    bool removeEvent(const QnUuid& id);
    void clear();

    QModelIndex indexOf(const QnUuid& id) const;
    EventData getEvent(const QModelIndex& index) const;

    virtual QString timestampText(qint64 timestampMs) const;

    virtual void triggerDefaultAction(const EventData& event);
    virtual void triggerCloseAction(const EventData& event);
    virtual void triggerLinkAction(const EventData& event, const QString& link);

    virtual int eventPriority(const EventData& event) const;

    virtual void beforeRemove(const EventData& event);

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace
} // namespace client
} // namespace nx
