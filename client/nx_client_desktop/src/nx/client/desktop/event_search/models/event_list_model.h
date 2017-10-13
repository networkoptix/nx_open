#pragma once

#include <QtCore/QScopedPointer>
#include <QtCore/QAbstractListModel>

#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/scoped_model_operations.h>

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
    void clear();

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace
} // namespace client
} // namespace nx
