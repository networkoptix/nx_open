#pragma once

#include <QtCore/QAbstractListModel>

#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/data_structures/keyed_list.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace client {
namespace desktop {

class ProgressListModel:
    public ScopedModelOperations<QAbstractListModel>,
    public QnWorkbenchContextAware
{
   Q_OBJECT
   using base_type = ScopedModelOperations<QAbstractListModel>;

public:
    ProgressListModel(QObject* parent);
    virtual ~ProgressListModel() override = default;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;

    virtual bool setData(const QModelIndex& index, const QVariant& value,
        int role = Qn::DefaultNotificationRole) override;

private:
    utils::KeyList<QnUuid> m_activities;
};

} // namespace
} // namespace client
} // namespace nx
