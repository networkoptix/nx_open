#pragma once

#include <QtCore/QScopedPointer>
#include <QtCore/QAbstractListModel>

#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/scoped_model_operations.h>

namespace nx {
namespace client {
namespace desktop {

class AbstractEventListModel:
    public ScopedModelOperations<QAbstractListModel>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = ScopedModelOperations<QAbstractListModel>;

public:
    explicit AbstractEventListModel(QObject* parent = nullptr);
    virtual ~AbstractEventListModel() override;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual bool canFetchMore(const QModelIndex& parent = QModelIndex()) const override;
    virtual void fetchMore(const QModelIndex& parent = QModelIndex()) override;

protected:
    bool isValid(const QModelIndex& index) const;
    static QString debugTimestampToString(qint64 timestampMs);

    virtual QString timestampText(qint64 timestampMs) const;
};

} // namespace
} // namespace client
} // namespace nx
