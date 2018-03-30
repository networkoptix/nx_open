#pragma once

#include <QtCore/QScopedPointer>
#include <QtCore/QAbstractListModel>

#include <recording/time_period.h>
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
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    virtual bool canFetchMore(const QModelIndex& parent = QModelIndex()) const override;
    virtual void fetchMore(const QModelIndex& parent = QModelIndex()) override;

    virtual bool fetchInProgress() const;

    virtual bool isConstrained() const;

    const QnTimePeriod& relevantTimePeriod() const;
    void setRelevantTimePeriod(const QnTimePeriod& value);

signals:
    void fetchAboutToBeFinished(QPrivateSignal); //< Emitted before actual model update.
    void fetchFinished(bool cancelled, QPrivateSignal);

protected:
    bool isValid(const QModelIndex& index) const;
    virtual QString timestampText(qint64 timestampUs) const;

    virtual bool defaultAction(const QModelIndex& index);
    virtual bool activateLink(const QModelIndex& index, const QString& link);

    virtual void relevantTimePeriodChanged(const QnTimePeriod& previousValue);

    void beginFinishFetch();
    void endFinishFetch(bool cancelled = false);

private:
    QnTimePeriod m_relevantTimePeriod = QnTimePeriod::anytime();
};

} // namespace desktop
} // namespace client
} // namespace nx
