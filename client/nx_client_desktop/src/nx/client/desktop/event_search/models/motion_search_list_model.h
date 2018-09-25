#pragma once

#include <QtCore/QScopedPointer>
#include <QtCore/QAbstractListModel>

#include <core/resource/resource_fwd.h>
#include <recording/time_period_list.h>
#include <ui/workbench/workbench_context_aware.h>

#include <nx/client/desktop/event_search/models/abstract_search_list_model.h>

class QnTimePeriod;

namespace nx {
namespace client {
namespace desktop {

class MotionSearchListModel: public AbstractSearchListModel
{
    Q_OBJECT
    using base_type = AbstractSearchListModel;

public:
    explicit MotionSearchListModel(QObject* parent = nullptr);
    virtual ~MotionSearchListModel() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    int totalCount() const;

signals:
    void totalCountChanged(int value);

protected:
    virtual bool canFetch() const override;
    virtual void requestFetch() override;
    virtual void clearData() override;
    virtual void truncateToRelevantTimePeriod() override;
    virtual void truncateToMaximumCount() override;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
