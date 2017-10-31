#pragma once

#include <QtCore/QScopedPointer>
#include <QtCore/QAbstractListModel>
#include <QtGui/QPixmap>

#include <ui/workbench/workbench_context_aware.h>

#include <nx/client/desktop/ui/actions/action.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>
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
        qint64 timestamp = 0;
        QPixmap icon;
        QColor titleColor;
        bool removable = false;
        int helpId = -1;
        ui::action::IDType actionId = ui::action::NoAction;
        ui::action::Parameters actionParameters;
        QVariant extraData;
    };

public:
    explicit EventListModel(QObject* parent = nullptr);
    virtual ~EventListModel() override;

    virtual int rowCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    bool addEvent(const EventData& event);
    bool updateEvent(const EventData& event);
    bool removeEvent(const QnUuid& id);
    void clear();

    QModelIndex indexOf(const QnUuid& id) const;

    void defaultAction(const QnUuid& id);
    void closeAction(const QnUuid& id);
    void linkAction(const QnUuid& id, const QString& link);

protected:
    virtual QString timestampText(qint64 timestampMs) const;

    virtual void triggerDefaultAction(const EventData& event);
    virtual void triggerCloseAction(const EventData& event);
    virtual void triggerLinkAction(const EventData& event, const QString& link);

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace
} // namespace client
} // namespace nx
