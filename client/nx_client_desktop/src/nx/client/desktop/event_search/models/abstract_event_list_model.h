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

    // An alternative to fetchMore that does not insert new rows immediately but calls
    //   completionHandler instead. To finalize fetch commitPrefetch must be called.
    //   Must return false and do nothing if previous fetch is not committed yet.
    //   Some kind of fetched subset synchronization between several models is possible via
    //   keyLimitToSync/keyLimitFromSync arguments to completionHandler and commitPrefetch.
    using PrefetchCompletionHandler = std::function<void(qint64 keyLimitToSync)>;
    virtual bool prefetchAsync(PrefetchCompletionHandler completionHandler);
    virtual void commitPrefetch(qint64 keyLimitFromSync);

protected:
    bool isValid(const QModelIndex& index) const;

    virtual QString timestampText(qint64 timestampMs) const;
};

} // namespace
} // namespace client
} // namespace nx
