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
    explicit EventListModel(QObject* parent = nullptr);
    virtual ~EventListModel() override;

    virtual int rowCount(const QModelIndex& parent) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual bool removeEvent(const QnUuid& id);

protected:
    struct EventData
    {
        QnUuid id;
        QString title;
        QString description;
        QString toolTip;
        qint64 timestamp = 0;
        QPixmap icon;
        QColor titleColor;
        int helpId = -1;
        ui::action::IDType actionId = ui::action::NoAction;
        ui::action::Parameters actionParameters;
    };

protected:
    virtual bool addEvent(const EventData& event);
    virtual void clear();

    virtual QString timestampText(qint64 timestampMs) const;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace
} // namespace client
} // namespace nx
