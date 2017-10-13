#pragma once

#include <QtCore/QScopedPointer>
#include <QtCore/QAbstractListModel>
#include <QtGui/QPixmap>

#include <ui/workbench/workbench_context_aware.h>

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

protected:
    struct EventData
    {
        QnUuid id;
        QString title;
        QString description;
        qint64 timestamp;
        QPixmap icon;
        QColor titleColor;

        // TODO: #vktuin Actions
        // TODO: #vktuin What else? Resource list?
    };

protected:
    virtual bool addEvent(const EventData& event);
    virtual bool removeEvent(const QnUuid& id);
    virtual void clear();

    virtual QString timestampText(qint64 timestampMs) const;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace
} // namespace client
} // namespace nx
